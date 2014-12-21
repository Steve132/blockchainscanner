#ifndef MICROHTTPDPP_H
#define MICROHTTPDPP_H

#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include<functional>


typedef std::function<bool (const struct sockaddr*,socklen_t)> acceptfunctype;
typedef std::function<
			bool (struct MHD_Connection *,
			const std::string&,
			const std::string&,
			const std::string&,
			const char*,
			size_t*,
			void **)
		> respondfunctype;
				
struct MHD_Daemon* mhdpp_start_daemon(		unsigned int flags,
						int port,
						const acceptfunctype& accept,
						const respondfunctype& respond,
						...);

#endif