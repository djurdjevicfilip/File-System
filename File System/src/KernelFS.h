#pragma once
#include "part.h"
#include "Semaphore.h"
#include "file.h"
#include <map>
#include <string>
const unsigned int IndexEntrySizeBytes = 4;
const unsigned int FileEntrySizeBytes = 32;
const unsigned int FileName = 8;

struct FileInfo {
public:
	char* buff;
	unsigned int count;
	BytesCnt size;
	BytesCnt cnt;
	ClusterNo entry;
	char access;
	FileInfo(char*buff, unsigned int count, BytesCnt size,ClusterNo entry,BytesCnt cnt,char c) {
		this->buff = buff;
		this->count = count;
		this->size = size;
		this->cnt = cnt;
		this->entry = entry;
		access = c;
	}
};

class KernelFS {
public:
	KernelFS() {
		waitToMount = new Semaphore();
		waitToOpen = new Semaphore();
	}
	char mount(Partition* partition);
	char unmount();
	char format();
	char doesExist(char* fname,ClusterNo*,BytesCnt*,ClusterNo*,BytesCnt*);
	File* open(char* fname, char mode);
	char deleteFile(char* fname);
	FileCnt readRootDir();
	
	friend class File;

	static map<string, FileInfo*> openFileTable;

	static Partition* mountedPartition;
	~KernelFS() {
		delete waitToMount;
		delete waitToOpen;
		delete[] bitVector;
		delete[] diskBitVector;
	}
//private:

	ClusterNo findFreeCluster(char**);
	static int allocateCluster(ClusterNo cluster,char*);
	static char* allocFreeSpaceInIndex(ClusterNo cluster,int*);//Returns pointer to the buffer which 
	static ClusterNo findCluster(char* buffer,ClusterNo);
	static void turnToNameAndExt(char* fname,char** name,char** ext);
	static char* bitVector; //1 represents the allocated Clusters  by the directory for its ENTRIES that are not FULL
	int putEntry(char* fname, ClusterNo,BytesCnt*);
	File* createFile(char* fname,bool);//unconditional
	ClusterNo findFreeDirectoryCluster();
	int allocateFile(char* fname,ClusterNo*,BytesCnt*);
	int countEntriesInCluster(char*buffer);
	int fileExists(char* buffer, char* fname,ClusterNo*,BytesCnt*,BytesCnt*);
	bool entryEmpty(char* pointer);
	void deleteEntry(char* buff, int entry,unsigned int*);

	static bool empty;
	bool waiting = false;
	unsigned int count = 0;
	static std::mutex mtx;

	static char* diskBitVector;
	static std::mutex mutex;
	static std::mutex mtxf;
	static condition_variable cv;
	static condition_variable cvf;

	static condition_variable cvFile;
	static std::mutex mtxFile;
	static Semaphore* waitToMount,*waitToOpen;
	static ClusterNo clustersForBitVector;
};
