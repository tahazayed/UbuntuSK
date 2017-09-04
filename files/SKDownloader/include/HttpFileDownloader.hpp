#ifndef HTTPFILEDOWNLOADER_H
#define HTTPFILEDOWNLOADER_H


#include <string>
#include <curl/curl.h>
#include <curl/easy.h>

#include "spdlog/spdlog.h"

using namespace std;
namespace spd = spdlog;
namespace SK
{

class HttpFileDownloader
{
public:

    struct S_dl_byte_data
    {
        double new_bytes_received;  //from the latest request
        double existing_filesize;
    };
    S_dl_byte_data dl_byte_data, *pdl_byte_data;
    int do_dl(const string& Url,const string& FileName,const string& tempfilename);

    /** Default constructor */
    HttpFileDownloader(std::shared_ptr<spd::logger> logger);

    /** Default destructor */
    virtual ~HttpFileDownloader();

protected:


private:
    spd::logger* m_logger;

    static size_t dl_write(void *buffer, size_t size, size_t nmemb, void *stream);
    static int dl_progress(S_dl_byte_data* pdata,double dltotal,double dlnow,double ultotal,double ulnow);
};
}

#endif // HTTPFILEDOWNLOADER_H
