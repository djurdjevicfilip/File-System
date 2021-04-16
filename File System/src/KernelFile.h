#pragma once
#include "fs.h"
#include "part.h"
#include <vector>
#include <string>
class FileInfo;
class KernelFile {
private:
	//where the file is, etc...
	BytesCnt position=0;
	char* secondIndex=nullptr;
	char*buffer;
	char* usedBuffer;
	char* currBuffer;
	ClusterNo secondIndexNum = 0;

	FileInfo *info;
	ClusterNo currAlloc=0;
	std::string name;
	BytesCnt size;
	ClusterNo firstIndex;
	int allocate(BytesCnt, char*buffer);
public:
	static ClusterNo firstEntry;
	static ClusterNo secondEntry;
	static ClusterNo buffEntry;
	ClusterNo alloc = 0;
	friend class File;
	KernelFile();
	~KernelFile(); //zatvaranje fajla
	char write(BytesCnt, char* buffer);
	BytesCnt read(BytesCnt, char* buffer);
	char seek(BytesCnt);
	int eof();
	char truncate();
	BytesCnt getFileSize();
	static void deleteFile(char*);
	void setNameAndPosition(char*fname, bool);
};
