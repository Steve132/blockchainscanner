#include "microhttpdpp.hpp"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include<functional>
#include<unordered_map>
#include <fstream>
#include <iterator>
#include<string>
#include "mhttpdfiles.h"

static bool ahc_echo_accept(const struct sockaddr* sa,socklen_t sl)
{
	return true;
}

int main(int argc,
	 char ** argv) {
  struct MHD_Daemon * d;
  filecache fc;
  respondfunctype rfn=std::bind(&filecache::serve_cached_files,&fc,
					std::placeholders::_1,
					std::placeholders::_2,
					std::placeholders::_3,
					std::placeholders::_4,
					std::placeholders::_5,
					std::placeholders::_6,
					std::placeholders::_7);
  
  d = mhdpp_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
		       8080,
		       
		       &ahc_echo_accept,
		       rfn,
		       MHD_OPTION_END);
  if (d == NULL)
    return 1;
  (void) getchar ();
  MHD_stop_daemon(d);
  return 0;
}
