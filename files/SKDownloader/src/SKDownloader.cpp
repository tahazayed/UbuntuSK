#include <stdio.h>
#include <syslog.h>
#include <signal.h>
#include <getopt.h> //getopt: parsing cli arguments
#include <string.h>
#include <future>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <list>
#include <algorithm>

#include "HttpFileDownloader.hpp"
#include "VideoClient.hpp"
#include "json.hpp"
#include "Util.hpp"
#include "version.h"

#include "spdlog/spdlog.h"



using namespace std;
using namespace SK;
namespace spd = spdlog;



static int running = 0;
static int delay = 1;
//static int counter = 0;
static string conf_file_name = strdup("/home/pi/SKDownloader/SKDownloader.conf");
static char *pid_file_name = NULL;
static int pid_fd = -1;
static char *app_name = NULL;
static FILE *log_stream;
static SK::VideoClient* videoClient;
static std::mutex download_lock ;
static bool downloading = false;
static string downloadDomain;
static string baseMediaFolder;
static string logLevel;
static string serial;
std::shared_ptr<spd::logger> logger;
std::unique_ptr<Util> util=std::make_unique<Util>();



/**
 * \brief Read configuration from config file
 */
int read_conf_file(const int reload)
{
    std::string content;
    std::unique_ptr<Util> util=std::make_unique<Util>();
    util->readFile(conf_file_name, content);

    auto j = json::parse(content);

    downloadDomain        = j["DownloadDomain"].get<std::string>();


    baseMediaFolder       = j["BaseMediaFolder"].get<std::string>();
    logLevel              = j["LogLevel"].get<std::string>();

    content.clear();
    if (reload == 1)
    {
        syslog(LOG_INFO, "Reloaded configuration file %s of %s",
               conf_file_name,
               app_name);
    }
    else
    {
        syslog(LOG_INFO, "Configuration of %s read from file %s",
               app_name,
               conf_file_name);
    }

    return 1;


}


/**
 * \brief Callback function for handling signals.
 * \param	sig	identifier of signal
 */
void handle_signal(const int sig)
{
    if (sig == SIGINT)
    {
        fprintf(log_stream, "Debug: stopping daemon ...\n");
        /* Unlock and close lockfile */
        if (pid_fd != -1)
        {
            lockf(pid_fd, F_ULOCK, 0);
            close(pid_fd);
        }
        /* Try to delete lockfile */
        if (pid_file_name != NULL)
        {
            unlink(pid_file_name);
        }
        running = 0;
        /* Reset signal handling to default behavior */
        signal(SIGINT, SIG_DFL);
    }
    else if (sig == SIGHUP)
    {
        fprintf(log_stream, "Debug: reloading daemon config file ...\n");
        read_conf_file(1);
    }
    else if (sig == SIGCHLD)
    {
        fprintf(log_stream, "Debug: received SIGCHLD signal\n");
    }
}

/**
 * \brief This function will daemonize this app
 */
static void daemonize()
{
    pid_t pid = 0;
    int fd;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
    {
        exit(EXIT_FAILURE);
    }

    /* Success: Let the parent terminate */
    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
    {
        exit(EXIT_FAILURE);
    }

    /* Ignore signal sent from child to parent process */
    signal(SIGCHLD, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
    {
        exit(EXIT_FAILURE);
    }

    /* Success: Let the parent terminate */
    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/");

    /* Close all open file descriptors */
    for (fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--)
    {
        close(fd);
    }

    /* Reopen stdin (fd = 0), stdout (fd = 1), stderr (fd = 2) */
    stdin = fopen("/dev/null", "r");
    stdout = fopen("/dev/null", "w+");
    stderr = fopen("/dev/null", "w+");

    /* Try to write PID of daemon to lockfile */
    if (pid_file_name != NULL)
    {
        char str[256];
        pid_fd = open(pid_file_name, O_RDWR|O_CREAT, 0640);
        if (pid_fd < 0)
        {
            /* Can't open lockfile */
            exit(EXIT_FAILURE);
        }
        if (lockf(pid_fd, F_TLOCK, 0) < 0)
        {
            /* Can't lock file */
            exit(EXIT_FAILURE);
        }
        /* Get current PID */
        sprintf(str, "%d\n", getpid());
        /* Write PID to lockfile */
        write(pid_fd, str, strlen(str));
    }
}


/**
 * \brief Print help for this application
 */
static void print_help(void)
{
    printf("\n Usage: %s [OPTIONS]\n\n", app_name);
    printf("  Options:\n");
    printf("   -h --help                 Print this help\n");
    printf("   -b --build-date              Print app build-date\n");
    printf("   -c --conf_file filename   Read configuration from the file\n");
    printf("   -l --log_file  filename   Write logs to the file\n");
    printf("   -d --daemon               Daemonize this application\n");
    printf("   -p --pid_file  filename   PID file used by daemonized app\n");
    printf("   -v --version              Print app version\n");

    printf("\n");
}

static void sendKeepAlive(const unsigned int period_msecs, std::atomic<bool>& keep_running)
{
    const auto interval = std::chrono::milliseconds(period_msecs) ;

    while( keep_running )
    {

        videoClient->sendKeepAlive();


        std::this_thread::sleep_for(interval) ;
    }

}

static void getPlayList(const unsigned int period_msecs, std::atomic<bool>& keep_running)
{
    const auto interval = std::chrono::milliseconds(period_msecs) ;


    while( keep_running )
    {
        if( !downloading )
        {
            std::lock_guard<std::mutex> guard(download_lock) ;
            downloading = true;
            std::unique_ptr<HttpFileDownloader> httpFileDownloader=std::make_unique<HttpFileDownloader>(logger);

            list <std::string> lstFileNames,lstOldFileNames;
            list <VideoClient::Video> lstVideos;
            if(videoClient->getPlayList(std::ref(lstVideos)))
            {
                for (auto const& video : lstVideos)
                {
                    string FileName = video.VideoURLMP4;
                    std::size_t found = FileName.find_last_of("/");
                    FileName = baseMediaFolder+"/videos/"+FileName.substr(found+1);
                    lstFileNames.push_back(FileName);
                    lstFileNames.push_back(FileName+".dl");

                    if(!util->exists_test(FileName))
                    {
                        logger.get()->debug("FileName:{}",FileName);

                        int downloadStatus = httpFileDownloader->do_dl(downloadDomain+video.VideoURLMP4,
                                             FileName,FileName+".dl");

                        if(downloadStatus == 1)
                        {
                            videoClient->updateVideoDownload(video.ID);
                        }
                    }

                    FileName.clear();

                }

            }

            lstVideos.clear();
            lstVideos.empty();

            util->getFilesList(baseMediaFolder+"/videos/",std::ref(lstOldFileNames));
            for (auto const& oldFileName : lstOldFileNames)
            {
                if (std::find (lstFileNames.begin(), lstFileNames.end(), oldFileName)==lstFileNames.end() )
                {
                    util->deleteFile(oldFileName);
                    std::cout << "Deleted: " <<oldFileName<< std::endl;
                }
            }
            lstOldFileNames.clear();
            lstOldFileNames.empty();

            downloading = false;

        }
        std::this_thread::sleep_for(interval) ;

    }

}

/* Main function */
int main(int argc, char *argv[])
{
    static struct option long_options[] =
    {
        {"conf_file", required_argument, 0, 'c'},
        {"log_file", required_argument, 0, 'l'},
        {"help", no_argument, 0, 'h'},
        {"daemon", no_argument, 0, 'd'},
        {"pid_file", required_argument, 0, 'p'},
        {"version", no_argument, 0, 'v'},
        {"build-date", no_argument, 0, 'b'},
        {NULL, 0, 0, 0}
    };
    int value, option_index = 0;//, ret;
    char *log_file_name = NULL;
    int start_daemonized = 0;

    app_name = argv[0];


    /* Try to process all command line arguments */
    while ((value = getopt_long(argc, argv, "c:l:t:p:vbdh", long_options, &option_index)) != -1)
    {
        switch (value)
        {
        case 'c':
            conf_file_name = strdup(optarg);
            break;
        case 'l':
            log_file_name = strdup(optarg);
            break;
        case 'p':
            pid_file_name = strdup(optarg);
            break;
        case 'v':
            cout<<MYAPP_STRPRODUCTVERSION<<endl;
            spd::drop_all();
            return EXIT_SUCCESS;
        case 'b':
            cout<<util->get_builddate()<<endl;
            spd::drop_all();
            return EXIT_SUCCESS;
        case 'd':
            start_daemonized = 1;
            break;
        case 'h':
            print_help();
            spd::drop_all();
            return EXIT_SUCCESS;
        case '?':
            print_help();
            spd::drop_all();
            return EXIT_FAILURE;

        default:
            break;
        }
    }

    /* When daemonizing is requested at command line. */
    if (start_daemonized == 1)
    {
        /* It is also possible to use glibc function deamon()
         * at this point, but it is useful to customize your daemon. */
        daemonize();
    }

    /* Open system log and write message to it */
    openlog(argv[0], LOG_PID|LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Started %s", app_name);

    /* Daemon will handle two signals */
    signal(SIGINT, handle_signal);
    signal(SIGHUP, handle_signal);

    /* Try to open log file to this daemon */
    if (log_file_name != NULL)
    {
        log_stream = fopen(log_file_name, "w+");
        if (log_stream == NULL)
        {
            syslog(LOG_ERR, "Can not open log file: %s, error: %s",
                   log_file_name, strerror(errno));
            log_stream = stdout;
        }
    }
    else
    {
        log_stream = stdout;
    }

    /* Read configuration from config file */
    read_conf_file(0);


    std::string logPath="/home/pi/SKDownloader/logs/log";
    // Runtime log levels

    if(logLevel == "DEBUG")
    {
        spd::set_level(spd::level::debug);
    }
    else if(logLevel == "WARN")
    {
        spd::set_level(spd::level::warn);
    }
    else if(logLevel == "CRITICAL")
    {
        spd::set_level(spd::level::critical);
    }
    else
    {
        spd::set_level(spd::level::info);
    }
    logger = spd::rotating_logger_mt("SKDownloader", logPath, 1048576 * 5, 3);
    logger.get()->flush_on(spd::level::debug);


    util->getMyMachineId(serial);
    cout<<"Serial:"<<serial<<endl;
    logger.get()->debug("Serial:{}",serial);

    videoClient = new SK::VideoClient(serial, "meonly", "http://tmgholding-001-site9.dtempurl.com/clients",logger);
    std::atomic<bool> keep_running {true} ;
    std::thread(sendKeepAlive, 300000, std::ref(keep_running) ).detach();

    std::thread(getPlayList, 60000, std::ref(keep_running)).detach();
    /* This global variable can be changed in function handling signal */
    running = 1;

    /* Never ending loop of server */
    while (running == 1)
    {
        /* Debug print */
        /*ret = fprintf(log_stream, "Debug: %d\n", counter++);
        if (ret < 0)
        {
            syslog(LOG_ERR, "Can not write to log stream: %s, error: %s",
                   (log_stream == stdout) ? "stdout" : log_file_name, strerror(errno));
            break;
        }
        ret = fflush(log_stream);
        if (ret != 0)
        {
            syslog(LOG_ERR, "Can not fflush() log stream: %s, error: %s",
                   (log_stream == stdout) ? "stdout" : log_file_name, strerror(errno));
            break;
        }
        */

        /* TODO: dome something useful here */

        /*std::future<bool> fut = std::async(sendKeepAlive);
        std::cout << fut.get() << std::endl;
        */

        /* Real server should use select() or poll() for waiting at
         * asynchronous event. Note: sleep() is interrupted, when
         * signal is received. */
        sleep(delay);
    }

    keep_running = false;

    std::this_thread::sleep_for( std::chrono::milliseconds(500) ) ; // give them a bit of time to exit cleanly
    videoClient->~VideoClient();
    /* Close log file, when it is used. */
    if (log_stream != stdout)
    {
        fclose(log_stream);
    }

    /* Write system log and close it. */
    syslog(LOG_INFO, "Stopped %s", app_name);
    closelog();

    /* Free allocated memory */

    if (log_file_name != NULL)
        free(log_file_name);
    if (pid_file_name != NULL)
        free(pid_file_name);
    /*
    if (app_name != NULL)
    free(app_name);
    */
    // Release and close all loggers
    spd::drop_all();
    return EXIT_SUCCESS;
}
