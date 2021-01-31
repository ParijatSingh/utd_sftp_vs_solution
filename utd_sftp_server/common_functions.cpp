#include "common_functions.h"

char* getCmdOption(char** begin, char** end, const std::string& option)
{
    char** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

/* Function to split a string by a delimiter and return a vector of split strings
*/
std::vector<std::string> split(const std::string& str, const std::string& delim)
{
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos - prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
}

void writeFileBytes(std::ofstream& ofs, uint8_t* data, int length, int idx)
{
    if (ofs)
    {
        ofs.seekp(idx, ofs.beg);
        ofs.write((char*)data, length);
        ofs.flush();
    }
}

/* Function to get the directory listing. Uses c++17 directory iterator
*  Include experimental/filesystem for c++14
*/
std::vector<fileInfo> getDirectoryListing(char* dirName)
{
    std::vector<fileInfo> fileList;
    fileInfo thefile;
    for (const auto& entry : fs::directory_iterator(dirName))
    {
        thefile.size = entry.file_size();
        thefile.path = entry.path().generic_string();
        fileList.push_back({ (uint32_t)entry.file_size(), entry.path().generic_string(), entry.is_directory() });
    }
    return fileList;
}

std::string extractDirectory(const std::string& path)
{
    if (path.find_last_of('/'))  return path.substr(0, path.find_last_of('/') + 1);
    else return path.substr(0, path.find_last_of('\\') + 1);
}

std::string extractFilename(const std::string& path)
{
    if (path.find_last_of('/')) return path.substr(path.find_last_of('/') + 1);
    else return path.substr(path.find_last_of('\\') + 1);
}

std::string getMsgName(int msgcode)
{
    switch (msgcode)
    {
    case 1: return "LISTDIRREQ";
    case 2: return "LISTDIRRES";
    case 3: return "GETFILEREQ";
    case 4: return "GETFILERES";
    case 5: return "POSTFILEREQ";
    case 6: return "POSTFILERES";
    case 7: return "DATA";
    case 8: return "ERR";
    case 9: return "ACK";
    default:
        break;
    }
    return "";
}

void printClientHelp()
{
    std::cout << "\n\t\tList of avilable commands:" << std::endl;
    std::cout << "\t---------------------------------------" << std::endl;
    std::cout << "\t\tlistdir [DIR_NAME]" << std::endl;
    std::cout << "\t\tgetfile [file1;file2;file3] [OUT_DIR]" << std::endl;
    std::cout << "\t\tpostfile [file1;file2;file3]" << std::endl;
    std::cout << "\t\ttrace [on|off]" << std::endl;
    std::cout << "\t\thelp" << std::endl;
    std::cout << "\t\texit" << std::endl;
    std::cout << "\t---------------------------------------" << std::endl << std::endl;
}


void printClientHelpDetailed()
{
    std::cout << "\tList of avilable commands:" << std::endl;
    std::cout << "\t---------------------------------------" << std::endl;
    std::cout << "\t*****listdir [DIR_NAME]*****" << std::endl;
    std::cout << "\tLists the directory from the server." << std::endl;
    std::cout << "\tIf no directory name specified then it list the default root server directory." << std::endl;
    std::cout << "\tServer may respond with error if the directory is not found." << std::endl;
    std::cout << "\t---------------------------------------" << std::endl;
    std::cout << "\t*****getfile [file1;file2;file3] [OUT_DIR]******" << std::endl;
    std::cout << "\tDownloads from server one or more files separated by ';'." << std::endl;
    std::cout << "\tSpaces in file path are currently not supported." << std::endl;
    std::cout << "\tServer may respond with error in case of access violation." << std::endl;
    std::cout << "\tIf no error the files will be download to output directory as specified in the command." << std::endl;
    std::cout << "\t---------------------------------------" << std::endl;
    std::cout << "\t******postfile [file1;file2;file3]]******" << std::endl;
    std::cout << "\tUploads ';' separated list of files to server. Spaces in file path are currently not supported." << std::endl;
    std::cout << "\tServer may respond with error in case of write violation." << std::endl;
    std::cout << "\tIf no error the files will be uploaded to default root dorectory on server." << std::endl;
    std::cout << "\t---------------------------------------" << std::endl;
    std::cout << "\t******trace [on|off]]******" << std::endl;
    std::cout << "\tEnables/Disbles the tracing of data block when uploading or downloading files" << std::endl;
    std::cout << "\t---------------------------------------" << std::endl;
    std::cout << "\t******exit]******" << std::endl;
    std::cout << "\tQuits the client application." << std::endl;
    std::cout << "\t---------------------------------------" << std::endl << std::endl;
}
