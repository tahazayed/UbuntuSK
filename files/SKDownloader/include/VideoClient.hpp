using namespace std;
#ifndef VIDEOCLIENT_H
#define VIDEOCLIENT_H

#include <string>
#include <list>
#include <json.hpp>
#include <curl/curl.h>
#include <curl/easy.h>

#include "spdlog/spdlog.h"



namespace spd = spdlog;

// for convenience
using json = nlohmann::json;


namespace SK
{

class VideoClient
{
public:

    /* holder for curl fetch */
    struct curl_fetch_st
    {
        char *payload;
        size_t size;
    };
    /** Default constructor */
    VideoClient(const string& Serial, const string& Secret, const string& BaseAPIUrl, std::shared_ptr<spd::logger> logger);

    /** Default destructor */
    virtual ~VideoClient();
    struct Video
    {
        int ID;
        int Order;
        std::string VideoURLMP4;

    };


    bool sendKeepAlive();
    bool getPlayList(list <VideoClient::Video>& lstVideos);
    void updateVideoDownload(const long& VideoID);


protected:

private:
    string m_Serial; //!< Member variable "m_Serial"
    string m_Secret; //!< Member variable "m_Secret"
    string m_baseAPIUrl;
    const long VIDEO_ZERO=0;
    spd::logger* m_logger;
    CURLcode postAPIAsync(const string& Method, const string& Content, struct curl_fetch_st *fetch);
    void getVideoClient(const long& VideoID,string& content);
};
}
#endif // VIDEOCLIENT_H
