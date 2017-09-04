using namespace std;
#ifndef UTIL_H
#define UTIL_H


#include <string>
#include <list>
#include <dirent.h>
#include <sys/stat.h>


namespace SK
{
class Util
{
public:
    /** Default constructor */
    Util();
    /** Default destructor */
    virtual ~Util();
    void getFilesList(const string& Path,list <std::string>& lstFiles);
    bool exists_test (const std::string& name);
    void deleteFile (const std::string& filePath);
    void readFile(const std::string& filepath,std::string &buffer);

    void getMyMachineId(std::string &machineID);
    char* GetSystemOutput(char* cmd);
    std::string get_builddate(void);

protected:

private:
    //void getPSN(char *PSN);
};
}
#endif // UTIL_H
