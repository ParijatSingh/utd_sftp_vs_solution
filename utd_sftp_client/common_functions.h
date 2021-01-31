#include <string>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

struct fileInfo {
	uint32_t size;
	//char path[256];
	std::string path;
	bool dir;
};

char* getCmdOption(char** begin, char** end, const std::string& option);
bool cmdOptionExists(char** begin, char** end, const std::string& option);
void writeFileBytes(std::ofstream& ofs, uint8_t* data, int length, int idx);
std::vector<fileInfo> getDirectoryListing(char* dirName);
std::string extractDirectory(const std::string& path);
std::string extractFilename(const std::string& path);
std::vector<std::string> split(const std::string& str, const std::string& delim);
std::string getMsgName(int msgcode);
void printClientHelp();
void printClientHelpDetailed();
