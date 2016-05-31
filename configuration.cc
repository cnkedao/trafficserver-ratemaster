#include "configuration.h"
#include "arpa/inet.h"
#include <netdb.h>
#include <fstream>
#include <algorithm>
#include <vector>

namespace ATS {
  using namespace std;

  void ltrim_if(string& s, int (* fp) (int)) {
    for (size_t i = 0; i < s.size();) { 
      if (fp(s[i])) { 
		s.erase(i,1);
      } else  {
		break;
      }
    }
  }

  void rtrim_if(string& s, int (* fp) (int)) {
    for (size_t i = s.size() - 1; i >= 0; i--) { 
      if (fp(s[i])) { 
		s.erase(i,1);
      } else  {
		break;
      }
    }
  }

  void trim_if(string& s, int (* fp) (int)) { 
    ltrim_if(s, fp);
    rtrim_if(s, fp);
  }

  vector<string> tokenize(const string &s, int (* fp) (int)) {
    vector<string> r;
    string tmp;
    
    for (size_t i = 0; i < s.size(); i++) {
      if ( fp(s[i]) ) {	
		if ( tmp.size()) { 
		  r.push_back(tmp);
		  tmp = "";
		}
      } else {
		tmp += s[i];
      }
    }

    if ( tmp.size()  ) { 
      r.push_back(tmp);
    }

    return r;
  }

  int Configuration::Parse(const char * path) {
    //dbg("Parseing file [%s]", path);
    //Configuration * c = new Configuration();
    std::ifstream f;

    size_t lineno = 0;

    f.open(path, std::ios::in);

    if (!f.is_open()) {
      //dbg("could not open file [%s], skip",path);
      return 0;
    }
	char temp[100];
	memset(temp,'\0',sizeof(temp));
	char *p;
	char *key;
	char *bp;
	uint64_t ssize;
    while (!f.eof()) {
      std::string line;
      getline(f, line);     
      ++lineno;

      trim_if(line, isspace);
      if (line.size() == 0) 
		continue;
	  bp = (char *)line.c_str();
	  p = strstr(bp,"=");
	  if(p){
	    memset(temp,'\0',sizeof(temp));
	    strncpy(temp,bp,p - bp);
		//dbg("key:%s:", temp);
		key = (char *)malloc(sizeof(temp));
		memset(key,'\0',sizeof(key));
		strcpy(key,temp);
		memset(temp,'\0',sizeof(temp));
		strcpy(temp,p + 1);
		ssize = atol(temp);
		limitconf.insert(std::pair<const char *, uint64_t> (key,ssize));
		//dbg("value:%ld:", ssize);
	  }
	  //limitconf.insert( std::pair<const char *, uint64_t> (key_copy,state));
	  /*
      vector<string> v = tokenize(line, isspace);

      for(size_t i = 0; i < v.size(); i++ ) {
		string token = v[i];
		if (!token.size()) continue;
		if (token[0] == '#') break; 
		   
			dbg("token on line [%ld]: [%s]", lineno, v[i].c_str());
	  }*/
    }

    return 1;
  }
}
