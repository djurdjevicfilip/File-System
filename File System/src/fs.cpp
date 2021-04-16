#include "fs.h"
#include "part.h"
#include "KernelFS.h"
KernelFS* FS::myImpl=new KernelFS();
char FS::mount(Partition* partition) {
	return FS::myImpl->mount(partition);
}
char FS::unmount() {
	return FS::myImpl->unmount();
}
char FS::format() {
	return FS::myImpl->format();
}
char FS::doesExist(char* fname) {
	ClusterNo cn;
	BytesCnt bcnt;
	ClusterNo entry;
	BytesCnt b;
	return FS::myImpl->doesExist(fname,&cn,&bcnt,&entry,&b);
}
File* FS::open(char* fname, char mode) {
	return FS::myImpl->open(fname, mode);
}
FileCnt FS::readRootDir() {
	return FS::myImpl->readRootDir();
}