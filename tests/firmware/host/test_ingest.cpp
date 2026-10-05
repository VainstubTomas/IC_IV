#include "../../../firmware/mesh_comun/mesh_ingest.h"
#include <cassert>
#include <cstdio>
int main(){
 const char* good="{\"inserted\":1,\"duplicates\":0,\"errors\":0,\"message\":\"ok\"}";
 assert(acceptedIngestReply(201,good));assert(!acceptedIngestReply(200,good));
 assert(acceptedIngestReply(201,"{\"inserted\":0,\"duplicates\":1,\"errors\":0}"));
 const char* bad[]={"{}","null","broken","{\"inserted\":1,\"duplicates\":0,\"errors\":1}","{\"inserted\":1,\"duplicates\":0,\"errors\":[]}","{\"inserted\":1,\"duplicates\":0,\"errors\":false}","{\"inserted\":1,\"duplicates\":0}","{\"inserted\":1,\"duplicates\":1,\"errors\":0}","{\"inserted\":0,\"duplicates\":0,\"errors\":0}","{\"inserted\":\"1\",\"duplicates\":0,\"errors\":0}","{\"inserted\":1.5,\"duplicates\":0,\"errors\":0}","{\"inserted\":-1,\"duplicates\":2,\"errors\":0}"};
 for(auto body:bad)assert(!acceptedIngestReply(201,body));
 puts("REST: respuesta 201 numerica, error parcial, duplicado, campos ausentes y tipos invalidos OK");
}
