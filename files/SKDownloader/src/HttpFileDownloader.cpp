#include <iostream>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

#include "HttpFileDownloader.hpp"

#include "Util.hpp"

using namespace std;
using namespace SK;

HttpFileDownloader::HttpFileDownloader(
    std::shared_ptr<spd::logger> logger)
    :m_logger(logger.get())
{

}

HttpFileDownloader::~HttpFileDownloader()
{
    m_logger = NULL;
}

int HttpFileDownloader::dl_progress(S_dl_byte_data* pdata,double dltotal,double dlnow,double ultotal,double ulnow)
{
    /*dltotal := hacky way of getting the Content-Length ~ less hacky would be to first
    do a HEAD request & then curl_easy_getinfo with CURLINFO_CONTENT_LENGTH_DOWNLOAD*/
    if (dltotal && dlnow)
    {
        pdata->new_bytes_received=dlnow;
        dltotal+=pdata->existing_filesize;
        dlnow+=pdata->existing_filesize;
        printf(" dl:%3.0f%% total:%.0f received:%.0f\r",100*dlnow/dltotal, dltotal, dlnow); //shenzi prog-mon
        fflush(stdout);
    }
    return 0;
}

size_t HttpFileDownloader::dl_write(void *buffer, size_t size, size_t nmemb, void *stream)
{
    return fwrite(buffer, size, nmemb, (FILE*)stream);
}

int HttpFileDownloader::do_dl(const string& Url,const string& FileName, const string& tempfilename)
{
     std::unique_ptr<Util> util=std::make_unique<Util>();
    if(util->exists_test(FileName))
    {

        SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] FileName:{}  already exists!,rcode:{}"
                     , FileName
                     , -5);

        return -5;
    }

    SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] dl:{}"
                 , Url);

    CURLM *multi_handle;
    CURL *curl;
    FILE *fp;

    int retval=0;
    int handle_count=0;
    double dl_bytes_remaining, dl_bytes_received;
    S_dl_byte_data st_dldata= {0};
    char curl_error_buf[CURL_ERROR_SIZE]= {"meh"};
    long dl_lowspeed_bytes=1000, dl_lowspeed_time=10; /* 1KBs for 10 secs*/

    /*put something biG here, preferably on a server that you can switch off at will ;) */

    char url[Url.size()+1],outfilename[FileName.size()+1],filename[tempfilename.size()+1];//as 1 char space for null is also required
    strcpy(outfilename, FileName.c_str());
    strcpy(filename, tempfilename.c_str());
    strcpy(url, Url.c_str());

    struct stat st= {0};


    if (!(fp=fopen(filename, "ab")) || -1==fstat(fileno(fp), &st)) //append binary
    {
        SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] Failed to open FileName:{}, rcode:{}"
                     , FileName
                     , -1);
        return -1;
    }


    if (curl_global_init(CURL_GLOBAL_DEFAULT))
    {
        SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] curl_global_init(CURL_GLOBAL_DEFAULT) FileName:{}, rcode:{}"
                     , FileName
                     , -2);
        return -2;
    }
    if (!(multi_handle = curl_multi_init()))
    {
        SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] !(multi_handle = curl_multi_init()) FileName:{}, rcode:{}"
                     , FileName
                     , -3);
        return -3;
    }

    if (!(curl = curl_easy_init()))
    {
        SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] !(curl = curl_easy_init()) FileName:{}, rcode:{}"
                     , FileName
                     , -4);
        return -4;
    }


    st_dldata.new_bytes_received=st_dldata.existing_filesize=st.st_size;

    //http://curl.haxx.se/libcurl/c/curl_easy_setopt.html
    curl_easy_setopt(curl, CURLOPT_URL, url);

    /*callbacks*/
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dl_write);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, dl_progress);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &st_dldata);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);

    /*curl will keep running -so you have the freedom to recover from network disconnects etc
    in your own way without distrubing the curl task in hand. ** this is by design :p **
    The follwoing sets up min download speed threshold & time endured before aborting*/
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, dl_lowspeed_bytes); //bytes/sec
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, dl_lowspeed_time); //seconds while below low spped limit before aborting


    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error_buf);


    do
    {
        if (st_dldata.new_bytes_received) //set the new range for the partial transfer if we have previously received some bytes
        {

            SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] FileName:{}, resuming d/l..."
                         , FileName);
            fflush(fp);
            //get the new filesize & sanity check for file; on error quit outer do-loop & return to main
            if (-1==(retval=fstat(fileno(fp), &st)) || !(st_dldata.existing_filesize=st.st_size))
                break;
            //see also: CURLOPT_RANGE for passing a string with our own X-Y range
            curl_easy_setopt(curl, CURLOPT_RESUME_FROM, st.st_size);
            st_dldata.new_bytes_received=0;
        }

        SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] FileName:{}, bytes already received:[{}]"
                     , FileName
                     , st_dldata.existing_filesize);

        //re-use the curl handle again & again & again & again... lol
        curl_multi_add_handle(multi_handle, curl);

        do //curl_multi_perform event-loop
        {
            CURLMsg *pMsg;
            int msgs_in_queue;

            while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multi_handle, &handle_count));

            //check for any mesages regardless of handle count
            while(pMsg=curl_multi_info_read(multi_handle, &msgs_in_queue))
            {
                long http_response;

                if (CURLMSG_DONE != pMsg->msg)
                {

                    SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] FileName:{}, CURLMSG_DONE != pMsg->msg:[{}]"
                                 , FileName
                                 , pMsg->msg);
                }
                else
                {
                    SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] FileName:{}, pMsg->data.result:[{}] meaning:[{}]"
                                 , FileName
                                 , pMsg->data.result
                                 , curl_easy_strerror(pMsg->data.result));
                    if (CURLE_OK != pMsg->data.result)
                    {
                        SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] FileName:{}, curl_error_buf:[{}]"
                                     , FileName
                                     , curl_error_buf);
                    }

                    switch(pMsg->data.result)
                    {
                    case CURLE_OK: ///////////////////////////////////////////////////////////////////////////////////////

                        SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] FileName:{}, CURLE_OK:"
                                     , FileName);
                        curl_easy_getinfo(pMsg->easy_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &dl_bytes_remaining);
                        curl_easy_getinfo(pMsg->easy_handle, CURLINFO_SIZE_DOWNLOAD, &dl_bytes_received);
                        if (dl_bytes_remaining == dl_bytes_received)
                        {
                            rename(filename, outfilename);
                            retval=1;
                        }
                        else
                        {
                            SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] FileName:{}, ouch! st_dldata.new_bytes_received[{}]"
                                         , FileName
                                         , st_dldata.new_bytes_received);
                            SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] FileName:{}, ouch! dl_bytes_received[{}] dl_bytes_remaining[{}]"
                                         , FileName
                                         , dl_bytes_received
                                         , dl_bytes_remaining);

                            retval=dl_bytes_received < dl_bytes_remaining ? 0 : -5;
                        }
                        break; /////////////////////////////////////////////////////////////////////////////////////////////////

                    case CURLE_COULDNT_CONNECT:      //no network connectivity ?
                    case CURLE_OPERATION_TIMEDOUT:   //cos of CURLOPT_LOW_SPEED_TIME
                    case CURLE_COULDNT_RESOLVE_HOST: //host/DNS down ?
                        printf("CURMESSAGE switch handle_count:[%d]\n",handle_count);
                        break; //we'll keep trying

                    default://see: http://curl.haxx.se/libcurl/c/libcurl-errors.html
                        handle_count=0;
                        retval=-5;
                    };


                    //see: http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
                    curl_easy_getinfo(pMsg->easy_handle, CURLINFO_RESPONSE_CODE, &http_response);
                    printf("CURLINFO_RESPONSE_CODE HTTP:[%ld]\n", http_response);
                    switch(http_response)
                    {
                    case 0:   //eg connection down  from kick-off ~suggest retrying till some max limit
                    case 200: //yay we at least got to our url
                    case 206: //Partial Content
                        break;

                    case 416:
                        //cannot d/l range ~ either cos no server support
                        //or cos we're asking for an invalid range ~ie: we already d/ld the file
                        //printf("HTTP416: either the d/l is already complete or the http server cannot d/l a range\n");
                        retval=2;

                    default: //suggest quitting on an unhandled error
                        handle_count=0;
                        retval=-6;
                    };
                }
            }

            if (handle_count) //select on any active handles
            {
                fd_set fd_read= {0}, fd_write= {0}, fd_excep= {0};
                struct timeval timeout= {5,0};
                int select_retval;
                int fd_max;

                curl_multi_fdset(multi_handle, &fd_read, &fd_write, &fd_excep, &fd_max);
                if (-1 == (select_retval=select(fd_max+1, &fd_read, &fd_write, &fd_excep, &timeout)))
                {
                    //errno shall be set to indicate the error
                    fprintf(stderr, "yikes! select error :(\n");
                    handle_count=0;
                    retval=-7;
                    break;
                }
                else {/*check whatever*/}
            }

        }
        while (handle_count);

        curl_multi_remove_handle(multi_handle,curl);
        //printf("continue from here?");
        //getchar();
    }
    while(retval==0);

    curl_multi_cleanup(multi_handle);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (fp)
    {
        fclose(fp);
    }
    SPDLOG_DEBUG(m_logger, "[HttpFileDownloader::do_dl] FileName:{}, retval:{}"
                 , FileName
                 , retval);

    return retval;
}

