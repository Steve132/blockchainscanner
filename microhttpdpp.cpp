#include "microhttpdpp.hpp"

extern "C" 
{
	static int accept_cwrap(void * cls,
		    const struct sockaddr* sa,socklen_t sl)
	{
		const acceptfunctype& af=*reinterpret_cast<const acceptfunctype*>(cls);
		if(af(sa,sl))
		{
			return MHD_YES;
		}
		else
		{
			return MHD_NO;
		}
	}
	static int respond_cwrap(void * cls,
		    struct MHD_Connection * connection,
		    const char * url,
		    const char * method,
                    const char * version,
		    const char * upload_data,
		    size_t * upload_data_size,
                    void ** ptr)
	{
		const respondfunctype& rf=*reinterpret_cast<const respondfunctype*>(cls);
		if(rf(connection,url,method,version,upload_data,upload_data_size,ptr))
		{
			return MHD_YES;
		}
		else
		{
			return MHD_NO;
		}
	}
}
		
struct MHD_Daemon* mhdpp_start_daemon(		unsigned int flags,
						int port,
						const acceptfunctype& accept,
						const respondfunctype& respond,
						...)
{
	struct MHD_Daemon *daemon;
	va_list ap;
	va_start (ap, respond);
	daemon = MHD_start_daemon_va (flags, port, &accept_cwrap, 
		const_cast<void*>(reinterpret_cast<const void*>(&accept)), 
		&respond_cwrap, 
		const_cast<void*>(reinterpret_cast<const void*>(&respond)), ap);
	va_end (ap);
	return daemon;
}