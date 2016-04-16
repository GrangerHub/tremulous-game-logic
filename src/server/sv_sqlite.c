#include "sv_sqlite.h"

sqlite3      *sl = NULL;
const char   *sl_mem_name = NULL;
sqlite3_stmt *sl_selected_stmt = NULL;
int           sl_bind_offset = 1;
int           sl_result_offset = 0;

int sl_exec_w( const char *sql ) {
  char *errmsg;
  if( sqlite3_exec( sl, sql, NULL, NULL, &errmsg ) != SQLITE_OK ) {
    Com_Error( ERR_DROP, "db_exec error: %s", errmsg );
    sqlite3_free( errmsg );
    return 1;
  }
  return 0;
}

int sl_exec( char *data, int *steps ) {
  return sl_exec_w( data );
}

#ifdef MEMORY_DATABASE
int sl_mem_load( const char *uriname ) {
  sqlite3 *file;
  sqlite3_backup *back;
  if( sqlite3_open( ":memory:", &sl ) != SQLITE_OK ) {
    Com_Error( ERR_DROP, "sl_mem_load error: %s", sqlite3_errmsg( sl ) );
    sl_close_sl( );
    return 1;
  }
  if( sqlite3_open( uriname, &file ) != SQLITE_OK ) {
    Com_Error( ERR_DROP, "sl_mem_load error: %s", sqlite3_errmsg( file ) );
    sqlite3_close( file );
    sl_close_sl( );
    return 2;
  }
  if( ( back = sqlite3_backup_init( sl, "main", file, "main" ) ) == NULL ) {
    Com_Error( ERR_DROP, "sl_mem_load error: %s", sqlite3_errmsg( sl ) );
    sqlite3_close( file );
    sl_close_sl( );
    return 3;
  }
  if( sqlite3_backup_step( back, -1 ) != SQLITE_DONE ) {
    Com_Error( ERR_DROP, "sl_mem_load error: sqlite3_backup_step failed" );
    sqlite3_close( file );
    sl_close_sl( );
    return 4;
  }
  if( sqlite3_backup_finish( back ) != SQLITE_OK ) {
    Com_Error( ERR_DROP, "sl_mem_load error: sqlite3_backup_finish failed" );
    sqlite3_close( file );
    sl_close_sl( );
    return 5;
  }
  sqlite3_close( file );
  sl_mem_name = uriname;
  return 0;
}

int sl_mem_close( void ) {
  sqlite3 *file;
  sqlite3_backup *back;
  if( sqlite3_open( sl_mem_name, &file ) != SQLITE_OK ) {
    Com_Error( ERR_DROP, "sl_mem_close error: %s", sqlite3_errmsg( file ) );
    sqlite3_close( file );
    return 1;
  }
  if( ( back = sqlite3_backup_init( file, "main", sl, "main" ) ) == NULL ) {
    Com_Error( ERR_DROP, "sl_mem_close error: %s", sqlite3_errmsg( file ) );
    sqlite3_close( file );
    return 2;
  }
  if( sqlite3_backup_step( back, -1 ) != SQLITE_DONE ) {
    Com_Error( ERR_DROP, "sl_mem_close error: sqlite3_backup_step failed" );
    sqlite3_close( file );
    return 3;
  }
  if( sqlite3_backup_finish( back ) != SQLITE_OK ) {
    Com_Error( ERR_DROP, "sl_mem_close error: sqlite3_backup_finish failed" );
    sqlite3_close( file );
    return 4;
  }
  sqlite3_close( file );
  return 0;
}
#endif

int sl_prep( const char *sql, sqlite3_stmt **stmt ) {
  if( sqlite3_prepare_v2( sl, sql, strlen( sql ), stmt, NULL ) != SQLITE_OK ) {
    return 1;
  }
  return 0;
}

int sl_open( char *data, int *steps ) {
  if( sl != NULL ) {
    Com_Error( ERR_DROP, "sl_open error: This implementation only supports 1 open database at a time." );
    return 1;
  }
#ifdef MEMORY_DATABASE
  if( sl_mem_load( data ) != 0 ) {
    return 2;
  }
#else
  if( sqlite3_open( data, &sl ) != SQLITE_OK ) {
    Com_Error( ERR_DROP, "sl_open error: %s", sqlite3_errmsg( sl ) );
    sl_close_sl( );
    return 2;
  }
#endif
  {//Executions
    sl_execs_t *execs = sl_execs;
    sl_execs_t *execs_end = sl_execs + sl_execs_count;
    for( ; execs < execs_end; execs++ ) {
      sl_exec_w( *execs );
    }
  }
  {//Statements
    sl_statements_s *statements = sl_statements;
    sl_statements_s *statements_end = sl_statements + sl_statements_count;
    for( ; statements < statements_end; statements++ ) {
      if( *( statements->stmt ) != NULL ) {
        Com_Error( ERR_DROP, "sl_open error: stmt for '%s' is not NULL. Maybe there is a duplicate or it is not initialized.", statements->sqlstmt );
        continue;
      }
      if( sl_prep( statements->sqlstmt, statements->stmt ) != 0 ) {
        Com_Error( ERR_DROP, "sl_open error: Failed to prepare '%s'", statements->sqlstmt );
      }
    }
  }
  return 0;
}

int sl_close( char *data, int *steps ){
  {//Free statements
    sl_statements_s *statements = sl_statements;
    sl_statements_s *statements_end = sl_statements + sl_statements_count;
    for( ; statements < statements_end; statements++ ) {
      if( *( statements->stmt ) != NULL ) {
        sqlite3_finalize( *( statements->stmt ) );
        *( statements->stmt ) = NULL;
      }
    }
  }
#ifdef MEMORY_DATABASE
  sl_mem_close( );
#endif
  sl_close_sl( );
  return 0;
}

void sl_close_sl( void ) {
  sqlite3_close( sl );
  sl = NULL;
}

int sl_query( dbArray_t type, char *data, int *steps ) {
  if( type >= DB_COUNT ) {
    Com_Error( ERR_FATAL, "sl_query: Invalid query enum/type %d", (int)type );
    return 1;
  }
  if( type != DB_OPEN && sl == NULL ) {
    return 2;
  }
  return (*sl_queries[ type ])( data, steps );
}

int sl_bind( sqlite3_stmt *stmt, qboolean reset ) {
  if( stmt == NULL ) {
    return 1;
  }
  sl_selected_stmt = stmt;
  if( reset == qtrue ) {
    sqlite3_reset( stmt );
  }
  sl_bind_offset = 1;
  sl_result_offset = 0;
  return 0;
}

int sl_bind_blob( const void *value, int len ) {
  if( sqlite3_bind_blob( sl_selected_stmt, sl_bind_offset, value, len, SQLITE_STATIC ) != SQLITE_OK ){
    return 1;
  }
  sl_bind_offset++;
  return 0;
}

int sl_bind_double( double value ) {
  if( sqlite3_bind_double( sl_selected_stmt, sl_bind_offset, value ) != SQLITE_OK ) {
    return 1;
  }
  sl_bind_offset++;
  return 0;
}

int sl_bind_int( int value ) {
  if( sqlite3_bind_int( sl_selected_stmt, sl_bind_offset, value ) != SQLITE_OK ) {
    return 1;
  }
  sl_bind_offset++;
  return 0;
}

int sl_bind_int64( int64_t value ) {
  if( sqlite3_bind_int64( sl_selected_stmt, sl_bind_offset, ( int64_t )value ) != SQLITE_OK ) {
    return 1;
  }
  sl_bind_offset++;
  return 0;
}

int sl_bind_null( void ) {
  if( sqlite3_bind_null( sl_selected_stmt, sl_bind_offset ) != SQLITE_OK ) {
    return 1;
  }
  sl_bind_offset++;
  return 0;
}

int sl_bind_text( const char *value, int len ) {
  if( sqlite3_bind_text( sl_selected_stmt, sl_bind_offset, value, len, SQLITE_STATIC ) != SQLITE_OK ) {
    return 1;
  }
  sl_bind_offset++;
  return 0;
}

int sl_bind_text16( const void *value, int len ) {
  if( sqlite3_bind_text16( sl_selected_stmt, sl_bind_offset, value, len, SQLITE_STATIC ) != SQLITE_OK ) {
    return 1;
  }
  sl_bind_offset++;
  return 0;
}

int sl_bind_zeroblob( int len ) {
  if( sqlite3_bind_zeroblob( sl_selected_stmt, sl_bind_offset, len ) != SQLITE_OK ) {
    return 1;
  }
  sl_bind_offset++;
  return 0;
}

int sl_step( qboolean query ) {
  switch( sqlite3_step( sl_selected_stmt ) ) {
    case SQLITE_ROW:
      sl_result_offset = 0;
      return 0;
    case SQLITE_DONE:
      if( query == qtrue ) {
        return 0; }
      return 1;
    case SQLITE_CONSTRAINT:
      return 0;
    default:break;
  }
  return 2;
}

int64_t sl_changes( void ) {
  return sqlite3_changes( sl );
}

int64_t sl_lastrow( void ) {
  return sqlite3_last_insert_rowid( sl );
}

int sl_result_blob( void **value ) {
  int len;
  *value = ( void * )sqlite3_column_blob( sl_selected_stmt, sl_result_offset );
  if( !*value ) {
    return 0;
  }
  len = sqlite3_column_bytes( sl_selected_stmt, sl_result_offset );
  sl_result_offset++;
  return len;
}

double sl_result_double( void ) {
  double result = sqlite3_column_double( sl_selected_stmt, sl_result_offset );
  sl_result_offset++;
  return result;
}

int sl_result_int( void ) {
  int result = sqlite3_column_int( sl_selected_stmt, sl_result_offset );
  sl_result_offset++;
  return result;
}

int64_t sl_result_int64( void ) {
  int64_t result = sqlite3_column_int64( sl_selected_stmt, sl_result_offset );
  sl_result_offset++;
  return result;
}

int sl_result_text( char **value ) {
  int len;
  *value = ( char * )sqlite3_column_text( sl_selected_stmt, sl_result_offset );
  if( !*value ) {
    return 0;
  }
  len = sqlite3_column_bytes( sl_selected_stmt, sl_result_offset );
  sl_result_offset++;
  return len;
}

int sl_result_text16( void **value ) {
  int len;
  *value = ( void * )sqlite3_column_text16( sl_selected_stmt, sl_result_offset );
  if( !*value ) {
    return 0;
  }
  len = sqlite3_column_bytes16( sl_selected_stmt, sl_result_offset );
  sl_result_offset++;
  return len;
}
