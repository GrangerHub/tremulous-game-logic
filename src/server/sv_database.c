#include "sv_sqlite.h"
#define TIME_OFFSET 1000000000
sl_execs_t sl_execs[ ] = {
  "CREATE TABLE IF NOT EXISTS map ( key INTEGER PRIMARY KEY, name TEXT, UNIQUE( name ) );",
  "CREATE TABLE IF NOT EXISTS mapstat ( mapkey INTEGER REFERENCES map( key ) ON DELETE CASCADE," //Foreign key instead of integer? Will need to enable it first.
    "result TEXT, players INTEGER, start INTEGER, end INTEGER );",
  "CREATE TABLE IF NOT EXISTS seen ( name TEXT, time INTEGER, UNIQUE( name ) );",
};
size_t sl_execs_count = SL_ARRAY_SIZE( sl_execs );

sqlite3_stmt	*add_map_stmt = NULL,
				*add_mapstat_stmt = NULL,
				*add_seen_stmt = NULL,
				*get_time_stmt = NULL,
				*get_map_key_stmt = NULL,
				*get_last_maps_stmt = NULL,
				*get_seen_stmt = NULL;
sl_statements_s sl_statements[ ] = {
  { "INSERT INTO map ( name ) VALUES ( ? );", &add_map_stmt },
  { "INSERT INTO mapstat ( mapkey, result, players, start, end ) VALUES ( ?, ?, ?, ?, strftime( '%s', 'now' ) );", &add_mapstat_stmt },
  { "INSERT OR REPLACE INTO seen ( name, time ) VALUES ( ?, strftime( '%s', 'now' ) );", &add_seen_stmt },
  { "SELECT strftime( '%s', 'now' );", &get_time_stmt },
  { "SELECT key FROM map WHERE name = ?;", &get_map_key_stmt },
  { "SELECT map.name, result, players, start, end, strftime( '%s', 'now' ) - end, datetime( end, 'unixepoch', 'utc' ) "
      "FROM mapstat JOIN map ON map.key = mapstat.mapkey WHERE players > ? ORDER BY end DESC LIMIT ? OFFSET ?;", &get_last_maps_stmt },
  { "SELECT name, strftime( '%s', 'now' ) - time, datetime( time, 'unixepoch', 'utc' ) FROM seen WHERE name like ? LIMIT ? OFFSET ?;", &get_seen_stmt } };
size_t sl_statements_count = SL_ARRAY_SIZE( sl_statements );

sl_queries_t sl_queries[ DB_COUNT ] = {
  [ DB_OPEN ] = sl_open,
  [ DB_CLOSE ] = sl_close,
  [ DB_EXEC ] = sl_exec,
  [ DB_MAPSTAT_ADD ] = db_add_mapstat,
  [ DB_SEEN_ADD ] = db_add_seen,
  [ DB_TIME_GET ] = db_get_time,
  [ DB_LAST_MAPS ] = db_get_last_maps,
  [ DB_SEEN ] = db_get_seen,
};
size_t sl_queries_count = SL_ARRAY_SIZE( sl_queries );

void hour_min( int64_t elapsed, int *hours,int *mins ) {
  *hours = elapsed / 3600;
  elapsed -= *hours * 3600;
  *mins = elapsed / 60;
}

int db_add_mapstat( char *data, int *steps ) {
  char   *name, *result;
  int     players, start;
  int64_t key, newstart;
  unpack_start( data, DATABASE_DATA_MAX );
  name = unpack_text3( );
  result = unpack_text3( );
  unpack_int( &players );
  unpack_int( &start );
  newstart = start + TIME_OFFSET;
  //Attempt to add the map.
  if( sl_bind( add_map_stmt, qtrue ) ||
      sl_bind_text( name, -1 ) ||
      sl_step( qtrue ) ) {
    Com_Error( ERR_DROP, "db_add_mapstat: Could not add map name %s", name );
    return 1;
  }
  //Query for map key.
  if( sl_bind( get_map_key_stmt, qtrue ) ||
      sl_bind_text( name, -1 ) ||
      sl_step( qfalse ) ) {
    Com_Error( ERR_DROP, "db_add_mapstat: Could not get map key for %s", name );
    return 1;
  }
  //Add the mapstat.
  key = sl_result_int64( );
  if( sl_bind( add_mapstat_stmt, qtrue ) ||
      sl_bind_int64( key ) ||
      sl_bind_text( result, -1 ) ||
      sl_bind_int( players ) ||
      sl_bind_int64( newstart ) ||
      sl_step( qtrue ) ) {
    Com_Error( ERR_DROP, "db_add_mapstat: Could not add mapstat for %s", name );
    return 2;
  }
  return 0;
}


int db_add_seen( char *data, int *steps ) {
  char *name = data;
  if( sl_bind( add_seen_stmt, qtrue ) ||
      sl_bind_text( name, -1 ) ||
      sl_step( qtrue ) ) {
    Com_Error( ERR_DROP, "db_add_seen: Could not add %s", name );
    return 1;
  }
  return 0;
}

int db_get_time( char *data, int *steps ) {
  if( sl_bind( get_time_stmt, qtrue ) ||
      sl_step( qtrue ) ) {
    Com_Error( ERR_DROP, "db_get_time: Could not get current time" );
    return 1;
  }
  pack_start( data, DATABASE_DATA_MAX );
  pack_int( (int)(sl_result_int64( ) - TIME_OFFSET) );
  return 0;
}

int db_get_last_maps( char *data, int *steps ) {
  int client_number, player_count, limit, offset;
  unpack_start( data, DATABASE_DATA_MAX );
  unpack_int( &client_number );
  unpack_int( &player_count );
  unpack_int( &limit );
  unpack_int( &offset );
  if( sl_bind( get_last_maps_stmt, qtrue ) ||
      sl_bind_int( player_count ) ||
      sl_bind_int( limit ) ||
      sl_bind_int( offset ) ) {
    Com_Error( ERR_DROP, "db_get_last_maps: Could not bind stmt" );
    return 1;
  }
  while( sl_step( qfalse ) == 0 ) {
	char *name, *result, *endstr;
	int players, hours, mins, elapsed_hours, elapsed_mins;
	int64_t start, end;
    sl_result_text( &name );
    sl_result_text( &result );
    players = sl_result_int( );
    start   = sl_result_int64( );
    end     = sl_result_int64( );
    hour_min( sl_result_int64( ), &elapsed_hours, &elapsed_mins );
    hour_min( end - start, &hours, &mins );
    sl_result_text( &endstr );
    if( client_number < 0 ) {
      Com_Printf( "%02d:%02d(%s) %s (%d players) (%02d:%02d playtime): %s\n", elapsed_hours, elapsed_mins, endstr, name, players, (int)hours, (int)mins, result );
    }
    else {
      SV_AddServerCommand( svs.clients + client_number, va( "print \"%02d:%02d(%s) %s (%d players) (%02d:%02d playtime): %s\n\"", elapsed_hours, elapsed_mins, endstr, name, players, (int)hours, (int)mins, result ) );
    }
  }
  return 0;
}

int db_get_seen( char *data, int *steps ) {
  int client_number, limit, offset;
  char *search;
  unpack_start( data, DATABASE_DATA_MAX );
  unpack_int( &client_number );
  search = unpack_text3( );
  unpack_int( &limit );
  unpack_int( &offset );
  if( sl_bind( get_seen_stmt, qtrue ) ||
      sl_bind_text( search, -1 ) ||
      sl_bind_int64( limit ) ||
      sl_bind_int64( offset ) ) {
    Com_Error( ERR_DROP, "db_get_seen: Could not bind stmt" );
    return 1;
  }
  while( sl_step( qfalse ) == 0 ) {
    char *name, *time;
    int elapsed_hours, elapsed_mins;
    sl_result_text( &name );
    hour_min( sl_result_int64( ), &elapsed_hours, &elapsed_mins );
    sl_result_text( &time );
    if( client_number < 0 ) {
      Com_Printf( "%02d:%02d(%s) : %s\n", elapsed_hours, elapsed_mins, time, name );
    }
    else {
      SV_AddServerCommand( svs.clients + client_number, va( "print \"%02d:%02d(%s) : %s\n\"", elapsed_hours, elapsed_mins, time, name ) );
    }
  }
  return 0;
}
