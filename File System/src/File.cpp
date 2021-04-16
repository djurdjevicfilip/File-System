#include "file.h"
#include "KernelFS.h"
#include "Semaphore.h"
File::File() {
	myImpl = new KernelFile();

}
int File::eof() {
	return myImpl->eof();
}
BytesCnt File::getFileSize() {
	return myImpl->getFileSize();
}
char File::seek(BytesCnt pos) {
	return myImpl->seek(pos);
}
BytesCnt File::filePos() {
	return myImpl->position;
}
BytesCnt File::read(BytesCnt bytes, char* buffer) {
	return myImpl->read(bytes, buffer);
}
char File::write(BytesCnt cnt, char* buffer) {
	return myImpl->write(cnt, buffer);
}
File::~File() { 
	string name = myImpl->name;
	delete[]myImpl->secondIndex;
	delete myImpl;
	FileInfo* dealloc = KernelFS::openFileTable.find(name)->second;
	dealloc->count--;
	if (dealloc->count == 0) {
		delete[] dealloc->buff;
		delete dealloc;
		std::unique_lock<std::mutex> lock(KernelFS::mtx);
		KernelFS::cv.notify_one();
		KernelFS::openFileTable.erase(name);
	}
	KernelFS::waitToOpen->notify(0);
	if (KernelFS::openFileTable.empty())KernelFS::empty = true;
	KernelFS::cv.notify_one();
	
	//KernelFS::mutex.unlock();
}