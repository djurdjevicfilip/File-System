#include "KernelFS.h"
#include <cmath>
map<string, FileInfo*> KernelFS::openFileTable;

Semaphore* KernelFS::waitToMount,* KernelFS::waitToOpen;

 condition_variable KernelFS::cv;
 std::mutex KernelFS::mtx;
 condition_variable KernelFS::cvFile;
 std::mutex KernelFS::mtxFile;

 std::mutex KernelFS::mutex;
 char* KernelFS::diskBitVector;
 condition_variable KernelFS::cvf;
 std::mutex KernelFS::mtxf;
ClusterNo KernelFS::clustersForBitVector;

Partition* KernelFS::mountedPartition = nullptr;
bool KernelFS::empty = true;
char* KernelFS::bitVector = nullptr;
//Thread protection for everything!!! This is really important.
char KernelFS::mount(Partition* partition) {
	if (false)return '0'; //in case of failure

	waitToMount->wait(0);

	mountedPartition = partition; //mounting the partition if possible
	return '1';
}

char KernelFS::unmount() {
	//When to return 0?
	//Block the caller until all the partition files are closed.
	//A simple semaphore should be sufficient
	if (waiting)return '0';
	std::unique_lock<std::mutex> lock(mtx);

	cout << (int)openFileTable.size() << endl;

	while (!openFileTable.empty()) {
		cout << "WAITING" << endl;
		waiting = true;
		cv.wait(lock);
	}
	mountedPartition = nullptr;
	waitToMount->notify(0);

	waiting = false;
	return '1';
}

char KernelFS::format() {
	if (mountedPartition == nullptr)return '0';

	//Block the thread if it needs to wait; If there is a waiting Thread, then return '0' for all the other tries
	if (!empty)return '0';
	std::unique_lock<std::mutex> lock(mtx);
	while (!openFileTable.empty()) {
		empty = false;
		cv.wait(lock);
	}

	
	//Initialize the bit vector and the Root Directory Index
	ClusterNo numberOfClusters = mountedPartition->getNumOfClusters();

	clustersForBitVector = ceil(numberOfClusters / (ClusterSize * 8.0));
	bitVector = new char[clustersForBitVector*ClusterSize]();
	
	
	unsigned char* buffer = new unsigned char[ClusterSize]{};

	int currMov = clustersForBitVector + 1;

	mountedPartition->writeCluster(clustersForBitVector, (char*)buffer);
	for(int j=0;j<clustersForBitVector;j++)
	for (int i = 0; i <ClusterSize; i++) {
		buffer[i] = ~0;
		if (currMov > 8) {
			buffer[i] = 0;
			currMov -= 8;
		}
		else {
			buffer[i] <<= currMov;
			currMov = 0;
		}
		mountedPartition->writeCluster(j, (char*)buffer);
	}
	

	for (int i = 0; i < ClusterSize; i++) {
		buffer[i] = ~0;

	}
	for (int i = 1; i < clustersForBitVector; i++)
		if (mountedPartition->writeCluster(i, (char*)buffer) == 0) {
			delete[] buffer;
			empty = false;
			return '0';
		}

	delete[] buffer;
	empty = true;
	delete[] diskBitVector;
	diskBitVector = new char[ClusterSize * clustersForBitVector];
	for (int i = 0; i < clustersForBitVector; i++)
		mountedPartition->readCluster(i, diskBitVector + i * ClusterSize);

	return '1';
}
//Works
char KernelFS::doesExist(char* fname,ClusterNo* cluster,BytesCnt* size,ClusterNo* entry,BytesCnt* cnt) {
	//We start at the clustersForBitVector position, there we can find the First Level Root Index

	string s = fname; 
	if (mountedPartition == nullptr) return '0';
	char* buffer = new char[ClusterSize] {};
	char* innerBuffer = new char[ClusterSize] {};
	char* fileBuffer = new char[ClusterSize] {};
	*entry = 0;
	mountedPartition->readCluster(clustersForBitVector, buffer);

	unsigned int* buffPointer = reinterpret_cast<unsigned int*>(buffer); //this is assuming an unsigned int is 4B
	
	while (buffPointer <reinterpret_cast<unsigned int*>(buffer + ClusterSize)) { //First-level
	
		if (*buffPointer == 0) {
			buffPointer++;
			continue;
		}
		ClusterNo nextPoint = *buffPointer;
		mountedPartition->readCluster(nextPoint, innerBuffer);

		unsigned int* innerPointer = reinterpret_cast<unsigned int*>(innerBuffer);

		while (innerPointer < reinterpret_cast<unsigned int*>(innerBuffer + ClusterSize)) { //Second-level
			if (*innerPointer == 0) {
				innerPointer++; continue;
			}

			ClusterNo entryCluster = *innerPointer;
			mountedPartition->readCluster(entryCluster, fileBuffer);
			//This checks the cluster
			if (fileExists(fileBuffer, fname, cluster,size,cnt)!=-1) {
				*entry = entryCluster;
				delete[] buffer;
				delete[] innerBuffer;
				delete[] fileBuffer;
				return '1';
			}
			innerPointer ++;
		}

		buffPointer++;
	}
	delete[] buffer;
	delete[] innerBuffer;
	delete[] fileBuffer;
	return '0';
}
int KernelFS::countEntriesInCluster(char* buffer) {
	char* it = buffer;
	//if (*it == 0)return 0;
	int count = 0;
	it += 8;//Offset, the first bytes for an entryCluster indicate the number of entries
	//cout << (int)*it << endl;

	while (it < buffer + ClusterSize) {
		if (entryEmpty(it))break;
		if (*it != 0) count++;
		it += 20;
	}
	return count;
}
int KernelFS::fileExists(char* buffer, char* fname, ClusterNo * fileCluster, BytesCnt * size, BytesCnt * cnt) {
	char* it = buffer;
	*cnt = 8;
	int entry = 0;

	char* name = new char[8];
	char* ext = new char[3];
	for (int i = 0; i < 8; i++) {
		name[i] = ' ';
		if (i < 3) ext[i] = ' ';
	}
	turnToNameAndExt(fname, &name, &ext);
	it += 8;//Offset, the first bytes for an entryCluster indicate the number of entries
	
	while (it < buffer + ClusterSize) {
		if (entryEmpty(it))break;
		if (*it == 0) { it += 20; (*cnt) += 20; entry++; continue; }
		char* ret = it;
		bool res = true;

		for (int i = 0; i < FNAMELEN; i++) {
			if (*it != name[i]) {
				res = false;
			}
			it++;
		}
		for (int i = 0; i < FEXTLEN; i++) {
			if (*it != ext[i]) {
				res = false;
				break;
			}
			it++;
		}
		it++;
		if (res) {
			unsigned int* uiit = reinterpret_cast<unsigned int*>(it);
			*fileCluster = *uiit;

			uiit++;
			*size = *uiit;
			delete[] name;
			delete[] ext;
			return entry;
		}
		entry++;
		(*cnt)+=20;
		it = ret + 20;
	}

	delete[] name;
	delete[] ext;
	return -1;
}
bool KernelFS::entryEmpty(char* pointer) {
	unsigned int* it = reinterpret_cast<unsigned int*>(pointer);
	bool res = true;
	for (int i = 0; i < 5; i++) {
		if (*it != 0) {
			res = false;
			break;
		}
		it++;
	}
	return res;
}
File* KernelFS::open(char* fname, char mode) {
	ClusterNo cn = 0;
	BytesCnt size = 0;
	ClusterNo entryPoint = 0;
	BytesCnt countPoint = 0;
	//mutex.lock();
	if ((mode == 'r' || mode == 'a') && doesExist(fname, &cn, &size, &entryPoint, &countPoint) == '0') { return nullptr; }//Error
	string s = fname;
	//Problem
	//mutex.unlock();
	while (mode == 'r' &&openFileTable.find(s) != openFileTable.end()&&openFileTable.find(s)->second->access=='w') {
		waitToOpen->wait(0);
	}
	while ((mode == 'w'||mode=='a') && openFileTable.find(s) != openFileTable.end()) {
		waitToOpen->wait(0);
	}

	doesExist(fname, &cn, &size, &entryPoint, &countPoint);
	//mutex.lock();
	File* f = nullptr;
	char* firstLevelIndex=new char[ClusterSize]();
	
	if (mode == 'w') {
		if (doesExist(fname,&cn,&size,&entryPoint,&countPoint)=='1') {
			deleteFile(fname); 
			if (openFileTable.find(s) != openFileTable.end()) {
				FileInfo* dealloc = openFileTable.find(s)->second;

					delete[] dealloc->buff;
					delete dealloc;
				openFileTable.erase(s);
			}
			size = 0;
		}
		ClusterNo alloc=0;
		BytesCnt cnt = 0;
		allocateFile(fname, &alloc, &cnt);
		string str = fname;
		openFileTable.insert(pair<string, FileInfo*>(str,new FileInfo(firstLevelIndex,1,size,alloc,cnt,'w')));
		
		f = createFile(fname, true);
	}
	else if (mode == 'r') {
		auto found = openFileTable.find(s);
		if (found != openFileTable.end()) {
			FileInfo* info = found->second;
		
			info->count++;
			f = createFile(fname, true);
		}
		else  {
			char* buff = new char[ClusterSize];
			mountedPartition->readCluster(entryPoint, buff);
			int* point = reinterpret_cast<int*>(buff + countPoint + 12);
			mountedPartition->readCluster(*point, firstLevelIndex);
			delete[]buff;
			string str = fname;
			openFileTable.insert(pair<string, FileInfo*>(str, new FileInfo(firstLevelIndex, 1, size, entryPoint,countPoint,'r')));

			f = createFile(fname, true);
		}

	}
	else if (mode == 'a') {
		auto found = openFileTable.find(s);
		if (found != openFileTable.end()) {
			//found->second->count++;
			FileInfo* info = found->second;
			info->count = 1;
			f = createFile(fname, false); 
			
		}
		else{
			char* buff = new char[ClusterSize];
			mountedPartition->readCluster(entryPoint, buff);
			int* point = reinterpret_cast<int*>(buff + countPoint + 12);
			mountedPartition->readCluster(*point, firstLevelIndex);
			delete[]buff;
			string str = fname;
			openFileTable.insert(pair<string, FileInfo*>(str, new FileInfo(firstLevelIndex, 1, size,entryPoint,countPoint,'w')));
			f = createFile(fname, false);
		}
	}

	if (f == nullptr)delete[]firstLevelIndex;
//	mutex.unlock();
	return f;
}
File* KernelFS::createFile(char* fname,bool start) {
	if (mountedPartition == nullptr)return nullptr;
	
	File* f = new File();
	f->myImpl->setNameAndPosition(fname, start);

	return f;
}
//Works
int KernelFS::allocateFile(char* fname,ClusterNo* alloc,BytesCnt* cnt) {
	ClusterNo toAlloc = findFreeDirectoryCluster();
	//cout << "toAlloc: " << toAlloc << endl;
	if (toAlloc != 0) {
		putEntry(fname, toAlloc,cnt);
		*alloc = toAlloc;
		return 1;
	}

	toAlloc = findCluster(diskBitVector,0); //cluster for the file
	if (toAlloc != 0) {
		if (allocateCluster(toAlloc,diskBitVector) == 0) {//Failed to allocate or find free index
			return 0;
		}
		putEntry(fname, toAlloc,cnt); 
		*alloc = toAlloc;
		//delete[] diskBitVector;
		return 1;
	}
	//delete[] diskBitVector;
	return 0;
}
//Works
//For the directory
int KernelFS::allocateCluster(ClusterNo cluster,char* diskBitVector) {
	int entryPosition;
	
	char* buffer = allocFreeSpaceInIndex(cluster,&entryPosition);

	//Second level
	if (buffer == nullptr) {//Counter-intuitive, but it's correct

		diskBitVector[cluster / 8] &= ~(1 << cluster % 8);
		//Clearing the cluster

	//	cout << "ALLOC" << endl;
		buffer = new char[ClusterSize]{};
		mountedPartition->writeCluster(cluster, buffer);

		delete[] buffer;
		//for (int i = 0; i < clustersForBitVector; i++)mountedPartition->writeCluster(i, diskBitVector);
		return 1;
	}
	//First AND Second level
	if (entryPosition != -1) {
		ClusterNo toAlloc = findCluster(diskBitVector,cluster);
		if (toAlloc == 0) {
			delete[] buffer;
			return 0;
		}
		*(buffer + entryPosition*IndexEntrySizeBytes) = toAlloc;
		mountedPartition->writeCluster(clustersForBitVector, buffer);
		mountedPartition->readCluster(toAlloc, buffer);

		diskBitVector[toAlloc / 8] &= ~(1 << toAlloc % 8);
		diskBitVector[cluster / 8] &= ~(1 << cluster % 8);
		delete[] buffer;
		//Clearing the cluster
		
		buffer = new char[ClusterSize] {};
		mountedPartition->writeCluster(cluster, buffer);
		//Clearing the second level index, but also putting the cluster at the first position of the index
		*buffer = cluster;
		mountedPartition->writeCluster(toAlloc, buffer);

		delete[] buffer;
		return 1;
	}
	delete[] buffer;
	return 0;
}
//Works
char* KernelFS::allocFreeSpaceInIndex(ClusterNo cluster,int* entryPosition) { //Refers to the Second level index of the directory
	//First search the index for free entries
	if (mountedPartition == nullptr) return nullptr;

	*entryPosition = -1;
	
	char* buffer = new char[ClusterSize] {};
	char* innerBuffer = new char[ClusterSize] {};

	mountedPartition->readCluster(clustersForBitVector, buffer);

	unsigned int* buffPointer = reinterpret_cast<unsigned int*>(buffer); //this is assuming an unsigned int is 4B
	int currPosition = 0;
	while (buffPointer < reinterpret_cast<unsigned int*>(buffer + ClusterSize)) { //First-level
		
		if (*buffPointer == 0) {
			if (*entryPosition == -1)*entryPosition = currPosition;
			currPosition++;
			buffPointer++;
			continue;
		}
		ClusterNo nextPoint = *buffPointer;
		mountedPartition->readCluster(nextPoint, innerBuffer);

		unsigned int* innerPointer = reinterpret_cast<unsigned int*>(innerBuffer);
		int pos = 0;
		while (innerPointer < reinterpret_cast<unsigned int*>(innerBuffer + ClusterSize)) { //Second-level
		
			if (*innerPointer == 0) {
				//Allocate cluster
				*innerPointer = (unsigned int)cluster;
				//Entry cluster

				mountedPartition->writeCluster(nextPoint, innerBuffer);
				delete[] buffer;
				delete[] innerBuffer;
				return nullptr;
			}
			pos++;
			innerPointer ++;
		}
		currPosition++;
		buffPointer ++;
	}
	delete[] innerBuffer;
	return buffer;
}
//If it can't fit it shouldn't be marked as Free!
//Works!
int KernelFS::putEntry(char* fname, ClusterNo cluster,BytesCnt* cnt) {
	char* buff = new char[ClusterSize];
	*cnt = 8;
	mountedPartition->readCluster(cluster, buff);
	char* it = buff;
	it += 8;// Offset

	char* name = new char[8];
	char* ext = new char[3];
	for (int i = 0; i < 8; i++){
		name[i] = ' ';
		if(i<3) ext[i] = ' ';
	}
	turnToNameAndExt(fname, &name, &ext);
	while (it < buff + ClusterSize) {
		if (*it == 0) { //Is free
			//cout << "ENTRY: " << *cnt << endl;
			for (int i = 0; i < 8;i++) {
				*it = *(name + i);

				it++;
			}
			for (int i = 0; i < 3; i++) {
				*it = *(ext + i);
				it++;
			}
			//1B
			*it = 0;
			it++;
			//Initially empty
			for (int i = 0; i < 8; i++) {
				*it = 0;
				it++;
			}

			if (*buff + 1 == 102) { //Reached the maximum capacity
				bitVector[cluster/8] &= ~(1 << cluster%8);
			}
			else { //Increment
				if(*buff==0)bitVector[cluster / 8] |= (1 << cluster % 8);
				(*buff)++;
			}
			break;
		}
		it += 20;
		(*cnt)+=20;
	}
	mountedPartition->writeCluster(cluster, buff); //CHECK ALL OF THIS, WRITE
	delete[] name;
	delete[] ext;
	delete[] buff;
	return 1;
}
int getBit(int n, int k) {//kth from n
	int mask = 1 << k;
	int masked = n & mask;
	return masked >> k;
}
ClusterNo KernelFS::findFreeCluster(char** diskBitVector) {
	return findCluster(*diskBitVector,0);
}

ClusterNo KernelFS::findCluster(char* buffer,ClusterNo cluster) { //Find cluster different from cluster, 0 if you can search for any
	char* pointer = buffer;
	ClusterNo byte = 0;
	while (pointer<buffer + ClusterSize*clustersForBitVector) {
		if (*pointer != 0) {
			for (int i = 0; i<8; i++)
				if (getBit(*pointer, i)) {
					if(byte*8+i!=cluster)
						return byte * 8 + i;
				}
		}
		byte++;
		pointer++;
	}
	return 0;
}

//Works
ClusterNo KernelFS::findFreeDirectoryCluster() { //Returns 0 if there is no free Cluster 
	if (bitVector == nullptr)return 0;
	return findCluster(bitVector,0);
}
//Works
void KernelFS::turnToNameAndExt(char* fname, char** name, char** ext) {
	bool isName = true;
	int nameCnt = 0, extCnt = 0;
	int i = 1;
	while(fname[i]!='\0'&&i<13){
		if (fname[i] == '.') {
			isName = false;
			i++;
			continue;
		}
		if (isName == true)(*name)[nameCnt++] = fname[i];
		else if(extCnt<3)(*ext)[extCnt++] = fname[i];
		i++;
	}
}
char KernelFS::deleteFile(char* fname) {
	//DELETE FROM OPEN FILE TABLE
	//WHAT IF THE ENTRY CLUSTER IS EMPTY AFTER DELETION????? I think this is solved, but still test it!!!
	if (mountedPartition == nullptr) return '0';
	//mutex.lock();
	ClusterNo cn;
	BytesCnt size;
	char* buffer = new char[ClusterSize] {};
	char* innerBuffer = new char[ClusterSize] {};
	char* fileBuffer = new char[ClusterSize] {};

	mountedPartition->readCluster(clustersForBitVector, buffer);

	unsigned int* buffPointer = reinterpret_cast<unsigned int*>(buffer); //this is assuming an unsigned int is 4B
	
	while (buffPointer < reinterpret_cast<unsigned int*>(buffer + ClusterSize)) { //First-level
		if (*buffPointer == 0) {
			buffPointer++;

			continue;
		}
		ClusterNo nextPoint = *buffPointer;
		mountedPartition->readCluster(nextPoint, innerBuffer);

		unsigned int* innerPointer = reinterpret_cast<unsigned int*>(innerBuffer);
		
		while (innerPointer < reinterpret_cast<unsigned int*>(innerBuffer + ClusterSize)) { //Second-level
			if (*innerPointer == 0) {
				innerPointer++; continue;
			}
			ClusterNo entryCluster = *innerPointer;
			mountedPartition->readCluster(entryCluster, fileBuffer);

			unsigned int* uiint = reinterpret_cast<unsigned int*>(fileBuffer);
			//This checks the cluster
			BytesCnt bytecnt;
			int entry = fileExists(fileBuffer, fname,&cn,&size,&bytecnt);

			if (entry != -1) {
				deleteEntry(fileBuffer, entry,innerPointer);
				mountedPartition->writeCluster(entryCluster, fileBuffer);
				string name = fname;
				if (KernelFS::openFileTable.find(name) != openFileTable.end()) {
					FileInfo* dealloc = KernelFS::openFileTable.find(name)->second;

					KernelFS::openFileTable.erase(name);
					delete[] dealloc->buff;
					delete dealloc;
					std::unique_lock<std::mutex> lock(KernelFS::mtx);
					KernelFS::cv.notify_one();
					KernelFS::openFileTable.erase(name);
				}
				delete[] buffer;
				delete[] innerBuffer;
				delete[] fileBuffer;
				return '1';
			}
			innerPointer++;
		}

		buffPointer++;
	}
	
	//mutex.unlock();
	return '0';
}
void KernelFS::deleteEntry(char* buff,int entry,unsigned int* innerPointer) {
	char* pointer = buff + 8 + 20 * entry;

	char* buffer = new char[ClusterSize]();
	char* buffer2 = new char[ClusterSize]();
	
	unsigned int* uiint = reinterpret_cast<unsigned int*>(buff + 20 * entry + 8 + 12);
	ClusterNo firstIndex =*uiint;
	mountedPartition->readCluster(firstIndex, buffer);
	unsigned int* buffFirst = reinterpret_cast<unsigned int*>(buffer);
	unsigned int* buffSecond;
	while (*buffFirst != 0) {
		KernelFS::mountedPartition->readCluster(*buffFirst, buffer2);
		buffSecond = reinterpret_cast<unsigned int*>(buffer2);
		while (*buffSecond != 0) {

			diskBitVector[*buffSecond / 8] |= (1 << *buffSecond % 8);
			buffSecond++;
		}

		diskBitVector[*buffFirst / 8] |= (1 << *buffFirst % 8);
		buffFirst++;
	}

	diskBitVector[firstIndex / 8] |= (1 << firstIndex % 8);
	delete[]buffer2;
	delete[]buffer;
	if ((pointer + 20) < buff + ClusterSize&&entryEmpty(pointer + 20)) {

		for (int i = 0; i < 5; i++) {
			*((unsigned int*)(pointer)) = 0;
			pointer += 20;
		}
	}
	else {
		*pointer = 0;
	}
	if (*buff - 1 == 0) {
		bitVector[entry / 8] &= ~(1 << entry% 8);
		//set in disk bit vector as free
		diskBitVector[entry / 8] |= 1 << entry % 8;

		*innerPointer = 0;
	}
	else {
		*buff = *buff - 1;
	}

}
//InnerBuffer vs InnerPointer
FileCnt KernelFS::readRootDir() {
	if (mountedPartition == nullptr) return -1;
	//mutex.lock();
	FileCnt count = 0;

	char* buffer = new char[ClusterSize] {};
	char* innerBuffer = new char[ClusterSize] {};
	char* fileBuffer = new char[ClusterSize] {};
	mountedPartition->readCluster(clustersForBitVector, buffer);
	unsigned int* buffPointer = reinterpret_cast<unsigned int*>(buffer); //this is assuming an unsigned int is 4B

	while (buffPointer < reinterpret_cast<unsigned int*>(buffer + ClusterSize)) { //First-level
		if (*buffPointer == 0) {
			buffPointer++;
			continue;
		}
	//	cout << *buffPointer << endl;
		ClusterNo nextPoint = *buffPointer;
		mountedPartition->readCluster(nextPoint, innerBuffer);
		unsigned int* innerPointer = reinterpret_cast<unsigned int*>(innerBuffer);
		
		int pos = 0;
		while (innerPointer < reinterpret_cast<unsigned int*>(innerBuffer + ClusterSize)) { //Second-level

			if (*innerPointer == 0) { innerPointer++; continue; }
			ClusterNo entryCluster = *innerPointer;
			cout << "here "<<entryCluster << endl;
			mountedPartition->readCluster(entryCluster, fileBuffer);
			count += countEntriesInCluster(fileBuffer);
			innerPointer++;
		}
		buffPointer++;
	}
	delete[] innerBuffer;
	delete[] fileBuffer;
	delete[] buffer;
//	mutex.unlock();
	return count;
}