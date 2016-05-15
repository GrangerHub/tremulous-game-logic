#include "q_shared.h"

char *pack_data;
int pack_max;

void pack_start( char *data, int max ) {
  pack_data = data;
  pack_max = max;
}

int pack_int( int in ) {
  if( pack_max < 4 ) {
    return 1;
  }
  *pack_data++ =   in         & 0xFF;
  *pack_data++ = ( in >>  8 ) & 0xFF;
  *pack_data++ = ( in >> 16 ) & 0xFF;
  *pack_data++ = ( in >> 24 ) & 0xFF;
  pack_max -= 4;
  return 0;
}

int pack_float( float in ) {
  int val = *( ( int * )&in );
  if( pack_max < 4 ) {
    return 1;
  }
  *pack_data++ =   val         & 0xFF;
  *pack_data++ = ( val >>  8 ) & 0xFF;
  *pack_data++ = ( val >> 16 ) & 0xFF;
  *pack_data++ = ( val >> 24 ) & 0xFF;
  pack_max -= 4;
  return 0;
}

int pack_text( char *in, int size ) {
  char *end = in + size;
  if( pack_max < size ) {
    return 1;
  }
  for( ; in < end; in++ ) {
    *pack_data++ = *in;
  }
  pack_max -= size;
  return 0;
}

int pack_text2( char *in ) {
  return pack_text( in, strlen( in ) + 1 );
}

char *unpack_data;
int unpack_max;

void unpack_start( char *data, int max ) {
  unpack_data = data;
  unpack_max = max;
}

int unpack_int( int *out ) {
  if( unpack_max < 4 ) {
    return 1;
  }
  *out = ( ( ( int )unpack_data[ 0 ] & 0xFF )       ) |
         ( ( ( int )unpack_data[ 1 ] & 0xFF ) <<  8 ) |
         ( ( ( int )unpack_data[ 2 ] & 0xFF ) << 16 ) |
         ( ( ( int )unpack_data[ 3 ] & 0xFF ) << 24 );
  unpack_data += 4;
  unpack_max -= 4;
  return 0;
}

int unpack_float( float *out ) {
  int ret;
  if( unpack_max < 4 ) {
    return 1;
  }
  ret = ( ( ( int )unpack_data[ 0 ] & 0xFF )       ) |
        ( ( ( int )unpack_data[ 1 ] & 0xFF ) <<  8 ) |
        ( ( ( int )unpack_data[ 2 ] & 0xFF ) << 16 ) |
        ( ( ( int )unpack_data[ 3 ] & 0xFF ) << 24 );
  *out = *( ( int * )&ret );
  unpack_data += 4;
  unpack_max -= 4;
  return 0;
}

int unpack_text( int size, char *out, int max) {
  char *end = unpack_data + size;
  if( size > max || unpack_max < size ) {
    return 1;
  }
  for( ; unpack_data < end; ) {
    *out++ = *unpack_data++;
  }
  unpack_max -= size;
  return 0;
}

int unpack_text2( char *out, int max ) {
  return unpack_text( strlen( unpack_data ) + 1, out, max );
}

char *unpack_text3( void ) {
  char *ret = unpack_data;
  int len = strlen( unpack_data ) + 1;
  if( len > unpack_max ) {
    return NULL;
  }
  unpack_data += len;
  return ret;
}
