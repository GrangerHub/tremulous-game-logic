#include <sqlite3.h>
#include "server.h"

extern sqlite3 *sl;
extern const char *sl_mem_name;

typedef const char *sl_execs_t;
extern sl_execs_t sl_execs[ ];
extern size_t sl_execs_count;

#define SL_ARRAY_SIZE( x ) ( sizeof( x ) / sizeof( x[ 0 ] ) )

typedef struct {
  const char *sqlstmt;
  sqlite3_stmt **stmt;
} sl_statements_s;
extern sl_statements_s sl_statements[ ];
extern size_t sl_statements_count;

typedef int (*sl_queries_t)( char *data, int *steps );
extern sl_queries_t sl_queries[DB_COUNT];
extern size_t sl_queries_count;

int sl_exec_w( const char *sql );
int sl_exec( char *data, int *steps );
int sl_mem_load( const char *uriname );
int sl_mem_close( void );
int sl_prep( const char *sql, sqlite3_stmt **stmt );
int sl_open( char *data, int *steps );
int sl_close( char *data, int *steps );
void sl_close_sl( void );
int sl_bind( sqlite3_stmt *stmt, qboolean reset );
int sl_bind_blob( const void *value, int len );
int sl_bind_double( double value );
int sl_bind_int( int value );
int sl_bind_int64( int64_t value );
int sl_bind_null( void );
int sl_bind_text( const char *value, int len );
int sl_bind_text16( const void *value, int len );
int sl_bind_zeroblob( int len );
int sl_step( qboolean query );
int64_t sl_changes( void );
int64_t sl_lastrow( void );
int sl_result_blob( void **value );
double sl_result_double( void );
int sl_result_int( void );
int64_t sl_result_int64( void );
int sl_result_text( char **value );
int sl_result_text16( void **value );

int db_add_mapstat( char *data, int *steps );
int db_add_seen( char *data, int *steps );
int db_get_time( char *data, int *steps );
int db_get_last_maps( char *data, int *steps );
int db_get_seen( char *data, int *steps );
