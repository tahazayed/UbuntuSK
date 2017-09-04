#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <memory>
#include <functional>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctime>

#include <stdio.h>

#include "Util.hpp"
#include "version.h"


using namespace std;
using namespace SK;



Util::Util()
{
    //ctor
}

Util::~Util()
{
    //dtor
}

void Util::getFilesList(const string& Path,list <std::string>& lstFiles)
{

    std::unique_ptr< DIR, std::function< int(DIR*) > > dir(opendir(Path.c_str()), closedir );
    struct dirent *file;

    std::string fileName;
    while ((file = readdir(dir.get())) != NULL)
    {
        fileName=string(file->d_name);

        if(fileName!="." && fileName!=".."  && fileName!="emptyfile.log")
        {
            lstFiles.push_back(Path+fileName);
        }
        fileName.clear();
    }
}

bool Util::exists_test (const std::string& name)
{
    struct stat buffer;
    return (stat (name.c_str(), &buffer) == 0);
}
void Util::deleteFile (const std::string& filePath)
{
    std::remove(filePath.c_str()); // delete file
}

void Util::readFile(const std::string& filepath,std::string &buffer)
{
    stringstream str;
    ifstream stream(filepath.c_str());
    if(stream.is_open())
    {
        while(stream.peek() != EOF)
        {
            str << (char) stream.get();
        }
        stream.close();
        buffer.assign(str.str());
    }
}
char* Util::GetSystemOutput(char* cmd)
{
    int buff_size = 32;
    char* buff = new char[buff_size];

    char* ret = NULL;
    string str = "";

    int fd[2];
    int old_fd[3];
    pipe(fd);

    old_fd[0] = dup(STDIN_FILENO);
    old_fd[1] = dup(STDOUT_FILENO);
    old_fd[2] = dup(STDERR_FILENO);

    int pid = fork();
    switch(pid)
    {
    case 0:
        close(fd[0]);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        dup2(fd[1], STDOUT_FILENO);
        dup2(fd[1], STDERR_FILENO);
        system(cmd);
        //execlp((const char*)cmd, cmd,0);
        close (fd[1]);
        exit(0);
        break;

    case -1:
        cerr << "GetSystemOutput/fork() error\n" << endl;
        exit(1);

    default:
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);

        int rc = 1;
        while (rc > 0)
        {
            rc = read(fd[0], buff, buff_size);
            str.append(buff, rc);
            //memset(buff, 0, buff_size);
        }

        ret = new char [strlen((char*)str.c_str())];

        strcpy(ret, (char*)str.c_str());

        waitpid(pid, NULL, 0);
        close(fd[0]);
    }

    dup2(STDIN_FILENO, old_fd[0]);
    dup2(STDOUT_FILENO, old_fd[1]);
    dup2(STDERR_FILENO, old_fd[2]);

    return ret;
}

void Util::getMyMachineId(std::string &machineID)
{
    machineID.assign(GetSystemOutput("grep -i serial /proc/cpuinfo | cut -d : -f2"));
    if(machineID=="")
    {
        machineID.assign(GetSystemOutput("cpuid | grep -m 1 \"serial number:\""));
    }
    if(machineID=="")
    {
        machineID.assign("0003-06A9-0000-0000-0000-0000");
    }
}

std::string Util::get_builddate()
{

    std::time_t unix_time = MYAPP_BUILDDATE;
    // return string(std::asctime(std::localtime(&unix_time)));
    struct tm * timeinfo;

    time ( &unix_time );
    timeinfo = localtime ( &unix_time );
    char output[32];

    strftime(output, 32, "%a, %d %b %Y %T %z", timeinfo);

    return string(output);
}
