#include "KernelFile.h"
#include "KernelFS.h"
KernelFile::KernelFile() {
	buffer = new char[ClusterSize] {};
	usedBuffer = new char[ClusterSize] {};
	currBuffer = new char[ClusterSize] {};
	secondIndex = new char[ClusterSize] {};
}
ClusterNo KernelFile::firstEntry = 0;
ClusterNo KernelFile::secondEntry;
ClusterNo KernelFile::buffEntry;
KernelFile::~KernelFile() {

	KernelFS::mountedPartition->writeCluster(firstIndex, info->buff);
	KernelFS::mountedPartition->writeCluster(currAlloc, currBuffer);
	char* entr = new char[ClusterSize];

	KernelFS::mountedPartition->readCluster(info->entry, entr);
	for (int i = info->cnt; i < info->cnt + 20; i++) { entr[i] = buffer[i]; }
	KernelFS::mountedPartition->writeCluster(info->entry, entr);
	info->size = size;

	delete[] entr;
	//if(secondIndex!=nullptr)
	//KernelFS::mountedPartition->writeCluster(secondIndexNum, secondIndex);
	
	char* pointer = buffer;
	pointer += (info->cnt + 12);
	unsigned int* point = reinterpret_cast<unsigned int*>(pointer);
	point++;
	delete[] buffer;
	delete[] usedBuffer;
	delete[] currBuffer;

}
//This is called during initialization
void KernelFile::setNameAndPosition(char*fname, bool start) {
	name = fname;
	string str = fname;
		info = KernelFS::openFileTable.find(str)->second;

		KernelFS::mountedPartition->readCluster(info->entry, buffer);
		char* buffe = new char[ClusterSize];
		KernelFS::mountedPartition->readCluster(info->entry, buffe);
		int* point = reinterpret_cast<int*>(buffe+ info->cnt + 12);
		firstIndex = *point;
		delete[]buffe;
	unsigned int* buff = reinterpret_cast<unsigned int*>(info->buff);
	size = info->size;
	if (start) {
		position = 0;	
	}
	else if(size>0){ 
		position = size;

	}

}

char KernelFile::write(BytesCnt bytes, char* bufferWrite) {
	if (bytes > 0) {
		if (size == 0) {
			//Allocate the first index

			ClusterNo toAlloc = KernelFS::findCluster(KernelFS::diskBitVector, 0);

			//Problem maybe if it's other than 0 and...
			if (toAlloc == 0) {
				//delete[] diskBitVector;

				return '0';
			}
			
			firstIndex = toAlloc;
			KernelFS::diskBitVector[toAlloc / 8] &= ~(1 << toAlloc % 8);

			//Clearing the cluster
			char* bufferr = new char[ClusterSize]();
			KernelFS::mountedPartition->writeCluster(toAlloc, bufferr);

			delete[] bufferr;
			//Don't write now for optimization
		//	for (int i = 0; i < KernelFS::clustersForBitVector; i++)KernelFS::mountedPartition->writeCluster(i, KernelFS::diskBitVector);
			if(info->buff==nullptr)
				info->buff = new char[ClusterSize]();
			KernelFS::mountedPartition->writeCluster(toAlloc, info->buff);
			//size += ClusterSize;
		}

		allocate(bytes,bufferWrite);
		//Write size into entry
		char* pointer = buffer;
		pointer += (info->cnt+12);
		unsigned int* point = reinterpret_cast<unsigned int*>(pointer);
		*point = firstIndex;
		point++;
		*point = size;
		info->size = size;
	
	}
	return '1';
}

int KernelFile::allocate(BytesCnt bytes, char*bufferWrite) {
	firstEntry = (position) / (ClusterNo)pow(2, 20);
	secondEntry = ((position) % (ClusterNo)pow(2, 20)) / (ClusterNo)pow(2, 11);
	buffEntry = ((position% (ClusterNo)pow(2, 20)) % (ClusterNo)pow(2, 11));
	BytesCnt written = 0;
	unsigned int* buffFirst = reinterpret_cast<unsigned int*>(info->buff);
	unsigned int* buffSecond = reinterpret_cast<unsigned int*>(info->buff);
	buffFirst += firstEntry;
	bool start = true;

	while (written < bytes) {
		if (*buffFirst == 0) {
			//Allocate the second index
			ClusterNo toAlloc = KernelFS::findCluster(KernelFS::diskBitVector, 0);
			if (toAlloc == 0) {
				return '0';
			}

			KernelFS::diskBitVector[toAlloc / 8] &= ~(1 << toAlloc % 8);

			delete[] usedBuffer;

			usedBuffer = new char[ClusterSize]();
			//Clearing the cluster
			KernelFS::mountedPartition->writeCluster(toAlloc, usedBuffer);

			*buffFirst = toAlloc;
			KernelFS::mountedPartition->writeCluster(firstIndex, info->buff);
		}
		KernelFS::mountedPartition->readCluster(*buffFirst, secondIndex);
		
			buffSecond = reinterpret_cast<unsigned int*>(secondIndex);
		if (start) {
			buffSecond += secondEntry;
			start = false;
		}
		if (*buffFirst != secondIndexNum) {
			if (secondIndexNum != 0) {
				KernelFS::mountedPartition->writeCluster(secondIndexNum, secondIndex);
			}
			secondIndexNum = *buffFirst;

			KernelFS::mountedPartition->readCluster(secondIndexNum, secondIndex);

		}
		while (buffSecond < reinterpret_cast<unsigned int*>(secondIndex + ClusterSize) && written < bytes) {

			ClusterNo toAlloc = *buffSecond;
			if (*buffSecond == 0) {
				//Allocate new buff

				toAlloc = KernelFS::findCluster(KernelFS::diskBitVector, 0);
				//Problem maybe if it's other than 0 and...
				if (toAlloc == 0) {
					return '0';
				}
				KernelFS::diskBitVector[toAlloc / 8] &= ~(1 << toAlloc % 8);
				//Clearing the cluster
				delete[] usedBuffer;

				usedBuffer = new char[ClusterSize]();
				KernelFS::mountedPartition->writeCluster(toAlloc, usedBuffer);
				KernelFS::mountedPartition->writeCluster(*buffFirst, secondIndex);
				*buffSecond = toAlloc;
				buffEntry = 0;
			}

			if (toAlloc != currAlloc) {
				if(currAlloc!=0)
					KernelFS::mountedPartition->writeCluster(currAlloc, currBuffer);
				currAlloc = toAlloc;
				KernelFS::mountedPartition->readCluster(toAlloc, currBuffer);
			
			}
			while (written < bytes && buffEntry < ClusterSize) {
				
				currBuffer[buffEntry++] = *(bufferWrite+written);
				written++;
				position++;//Position!!!
				
				size++;
			}

			//KernelFS::mountedPartition->writeCluster(toAlloc, currBuffer);
			buffEntry = 0;
			buffSecond++;
		}

		//KernelFS::mountedPartition->writeCluster(*buffFirst, secondIndex);
		buffFirst++;
	}
}

BytesCnt KernelFile::read(BytesCnt bytes, char* bufferRead) {
	if (KernelFS::openFileTable.find(name) == KernelFS::openFileTable.end())return 0;
	
	ClusterNo firstEntry = position / (ClusterNo)pow(2, 20);
	ClusterNo secondEntry = (position % (ClusterNo)pow(2, 20)) / (ClusterNo)pow(2, 11);
	ClusterNo buffEntry = (position % (ClusterNo)pow(2, 20)) % (ClusterNo)pow(2, 11);

	BytesCnt bytesRead = 0;
	
	unsigned int* buffFirst = reinterpret_cast<unsigned int*>(info->buff);
	
	unsigned int* buffSecond=reinterpret_cast<unsigned int*>(info->buff);
	buffFirst += firstEntry;
	bool start = true;
	while (bytesRead < bytes&&*buffFirst!=0) {
		if (*buffFirst != secondIndexNum) {
			secondIndexNum = *buffFirst;
			
			KernelFS::mountedPartition->readCluster(secondIndexNum, secondIndex);

		}
		buffSecond = reinterpret_cast<unsigned int*>(secondIndex);
		if (start) {
			buffSecond += secondEntry;
			start = false;
		}
		while (buffSecond < reinterpret_cast<unsigned int*>(secondIndex + ClusterSize) && bytesRead < bytes) {
		
				ClusterNo toAlloc = *buffSecond;
				if (toAlloc != currAlloc) {
					currAlloc = toAlloc;
					KernelFS::mountedPartition->readCluster(toAlloc, usedBuffer);

				}
				while (bytesRead < bytes && buffEntry < ClusterSize) {

					*(bufferRead + bytesRead) = usedBuffer[buffEntry++];
					bytesRead++;
					position++;

				}
			
			buffEntry = 0;
			buffSecond++;
		}
		buffFirst++;
	}
	return bytesRead;
}
char KernelFile::seek(BytesCnt pos) {
	if (size > pos) {
		position = pos;
		return '1';
	}
	else
		return '0';
}
int KernelFile::eof() {
	if (position == size) {
		return 1;
	}
	else
		return 0;
}
BytesCnt KernelFile::getFileSize() {
	
	return size;
	
}
