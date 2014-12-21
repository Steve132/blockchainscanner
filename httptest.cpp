#include "microhttpdpp.hpp"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include<functional>
#include<unordered_map>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <iterator>
#include<string>


static bool ahc_echo_accept(const struct sockaddr* sa,socklen_t sl)
{
	return true;
}
static bool def_reject(struct MHD_Connection * connection,
		    const std::string& url,
		    const std::string& method,
                    const std::string& version,
		    const std::string& upload_data,
		    size_t * upload_data_size,
                    void ** ptr) 
{
	return false;
}


time_t get_mtime(const std::string& path)
{
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) == -1) {
        return 0;
    }
    return statbuf.st_mtime;
}

std::string abspath(const std::string& s)
{
	char* rp=realpath(s.c_str(),NULL);
	std::string news(rp);
	free(rp);
	return news;
}

struct filecache
{
	struct filecachevalue_t
	{
		time_t lasttime;
		std::string contents;
	};
	std::unordered_map<std::string,filecachevalue_t> data;
	std::string folderbase;
	respondfunctype notfoundresponse;
	
	filecache(const std::string& fb=".",const respondfunctype& df=def_reject):
		folderbase(abspath(fb)),
		notfoundresponse(df)
	{}
	
	int serve_cached_files(struct MHD_Connection * connection,
		    std::string url,
		    const std::string& method,
                    const std::string& version,
		    const char* upload_data,
		    size_t * upload_data_size,
                    void ** ptr) 
	{
		static int dummy;			//Not reentrant?!
		int ret;

		if ( method != "GET")
			return MHD_NO; /* unexpected method */
			
			//TODO: check if URL exists in first call or have to defer file valid checks 
		
		if (&dummy != *ptr)
		{
			/* The first time only the headers are valid,
				do not respond in the first round... */
			*ptr = &dummy;
			return MHD_YES;
		}
		if (0 != *upload_data_size)
			return MHD_NO; /* upload data in a GET!? */
		*ptr = NULL; /* clear context pointer */
		
		/*if( url.find('.') !=std::string::npos)
		{
			return false;  //invalid get because get url can't contain '.'
		}*/
		
		if(url == "/")
		{
			url="/index.html";
		}
		
		std::string fn2(folderbase+url);
		auto iter=data.find(fn2);
		time_t mt;
		if(iter == data.end() || iter->second.lasttime < (mt=get_mtime(fn2)))
		{
			std::ifstream infile(fn2,std::ifstream::in | std::ifstream::binary);
			if(infile)
			{
				iter=data.emplace(fn2,filecachevalue_t{mt,std::string((std::istreambuf_iterator<char>(infile)),std::istreambuf_iterator<char>())}).first;
			}
		}
		if(iter == data.end())//file not found
		{
			std::string out("<html><body><h1>");
			out+="GET "+url+" :requested file not found</h1></body></html>";
			return mdhpp_respond(connection,out,MHD_HTTP_NOT_FOUND);//notfoundresponse(connection,url,method,version,upload_data,upload_data_size,ptr);
		}
		
		const std::string& content=iter->second.contents;

		return mdhpp_respond(connection,content,MHD_HTTP_OK,false);
	}
};
		


int main(int argc,
	 char ** argv) {
  struct MHD_Daemon * d;
  if (argc != 2) {
    printf("%s PORT\n",
	   argv[0]);
    return 1;
  }
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
