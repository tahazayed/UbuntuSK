#include "VideoClient.hpp"
#include <iostream>
using namespace std;
using namespace SK;


VideoClient::VideoClient(const string& Serial,
                         const string& Secret,
                         const string& BaseAPIUrl,
                         std::shared_ptr<spd::logger> logger)
    :m_Serial(Serial),m_Secret(Secret),m_baseAPIUrl(BaseAPIUrl),m_logger(logger.get())
{

}

VideoClient::~VideoClient()
{
    m_logger = NULL;
}

void VideoClient::getVideoClient(const long& VideoID,string& content)
{
    content.assign("{\"Serial\":\""+m_Serial+"\", \"Secret\":\""+m_Secret+"\", \"VideoID\":"+to_string(VideoID)+"}");
}

/* callback for curl fetch */
size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;                             /* calculate buffer size */
    struct VideoClient::curl_fetch_st *p = (struct VideoClient::curl_fetch_st *) userp;   /* cast pointer to fetch struct */

    /* expand buffer */
    p->payload = (char *) realloc(p->payload, p->size + realsize + 1);

    /* check buffer */
    if (p->payload == NULL)
    {
        /* free buffer */
        free(p->payload);
        /* return */
        return -1;
    }

    /* copy contents to buffer */
    memcpy(&(p->payload[p->size]), contents, realsize);

    /* set new buffer size */
    p->size += realsize;

    /* ensure null termination */
    p->payload[p->size] = 0;

    /* return size */
    return realsize;
}
CURLcode VideoClient::postAPIAsync(const string& Method, const string& Content,struct curl_fetch_st *fetch)
{

    CURLcode rcode;

    CURL *curl;

    struct curl_slist *headers;


    headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate");
    headers = curl_slist_append(headers, "Cache-Control: no-store, no-cache");

    /* init payload */
    fetch->payload = (char *) calloc(1, sizeof(fetch->payload));

    /* check payload */
    if (fetch->payload == NULL)
    {
        /* return error */
        return CURLE_FAILED_INIT;
    }

    /* init size */
    fetch->size = 0;
    string url=string( m_baseAPIUrl+Method);


    curl = curl_easy_init();

    /* set calback function */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, Content.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/7.38.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    /* pass fetch struct pointer */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) fetch);

    rcode = curl_easy_perform(curl);


    curl_easy_cleanup(curl);
    curl = NULL;
    curl_slist_free_all(headers);
    headers = NULL;
    return rcode;
}

bool VideoClient::sendKeepAlive()
{
    bool bResult = false;

    std::unique_ptr<curl_fetch_st> cf( new curl_fetch_st );
    string content;
    getVideoClient(VIDEO_ZERO,content);

    CURLcode rcode=postAPIAsync("/KeepAlive",content,cf.get());

    SPDLOG_DEBUG(m_logger, "[VideoClient::sendKeepAlive] rcode:{}"
                 , rcode);

    if(rcode==CURLE_OK)
    {
        auto j = json::parse(string(cf->payload));
        if(j["error"]=="OK")
        {
            bResult = true;
        }
        SPDLOG_DEBUG(m_logger, "[VideoClient::sendKeepAlive] Result:{}"
                     , j["error"].get<std::string>());
    }

    free((void*)(cf->payload));


    return bResult;
}

bool VideoClient::getPlayList(list <VideoClient::Video>& lstVideos)
{
    bool bResult = false;
    /* curl fetch struct */
    std::unique_ptr<curl_fetch_st> cf( new curl_fetch_st );

    string content;
    getVideoClient(VIDEO_ZERO,content);

    CURLcode rcode=postAPIAsync("/GetPlayList",content,cf.get());

    SPDLOG_DEBUG(m_logger, "[VideoClient::getPlayList] rcode:{}"
                 , rcode);

    if(rcode==CURLE_OK)
    {
        auto j = json::parse(string(cf->payload));
        if(j["Videos"].size()>0)
        {
            bResult = true;
        }
        for (json::iterator it = j["Videos"].begin(); it != j["Videos"].end(); ++it)
        {
            VideoClient::Video v
            {
                it[0]["ID"].get<int>(),
                it[0]["Order"].get<int>(),
                it[0]["VideoURLMP4"].get<std::string>()
            };
            lstVideos.push_back(v);
        }
    }

    free((void*)(cf->payload));

    SPDLOG_DEBUG(m_logger, "[VideoClient::getPlayList] lstVideos.size():{}"
                 , lstVideos.size());

    return bResult;

}

void VideoClient::updateVideoDownload(const long& VideoID)
{
    std::unique_ptr<curl_fetch_st> cf( new curl_fetch_st );

    string content;
    getVideoClient(VideoID,content);

    CURLcode rcode=postAPIAsync("/UpdateVideoDownload",content,cf.get());

    SPDLOG_DEBUG(m_logger, "[VideoClient::updateVideoDownload] rcode:{}"
                 , rcode);

    if(rcode==CURLE_OK)
    {
        auto j = json::parse(string(cf->payload));

        SPDLOG_DEBUG(m_logger, "[VideoClient::updateVideoDownload] Received:{}, Result:{}"
                     , VideoID
                     , j["error"].get<std::string>());
    }

    free((void*)(cf->payload));
}
