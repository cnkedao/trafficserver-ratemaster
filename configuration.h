#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <map>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "debug_macros.h"

namespace ATS{ 
  struct cmp_str {
	 bool operator()(char const *a, char const *b) {
	   return strcmp(a,b) < 0;
     }
  };
  class Configuration
  {
  public:
    int Parse(const char * path);
	explicit Configuration(){}
	std::map<const char *,uint64_t,cmp_str> limitconf;
	~Configuration() {
      std::map<const char *,uint64_t>::iterator it;
      for(it = limitconf.begin(); it != limitconf.end(); it++) {
		  free((void *)it->first);
      }
    }
  private:
    DISALLOW_COPY_AND_ASSIGN(Configuration);
  }; //class Configuration

}//namespace

#endif
