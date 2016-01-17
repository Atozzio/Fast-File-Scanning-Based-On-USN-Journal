
/**
* 对Ntfs下USN操作的示例程序
*
* author: univasity
* qq: 595920004
*
* --------------- 2010.11.10
* 1.修改lowUsn的问题，将原来的通过USN_JOURNAL获取，改为直接设置为0.
*
*/
#include <iostream>
#include <Windows.h>
#include <fstream>
#include <stdio.h>

using namespace std;

char* volName = "F:\\"; // 驱动盘名称

HANDLE hVol; // 用于储存驱动盘句柄

USN_JOURNAL_DATA UsnInfo; // 用于储存USN日志的基本信息

#define BUF_LEN 4096

ofstream fout("F:\\log.txt"); // 用来将数据记录到文本，方便查看

long counter = 0;

int main(){

	bool status;
	bool isNTFS = false;
	bool getHandleSuccess = false;
	bool initUsnJournalSuccess = false;


	/**
	* step 01. 判断驱动盘是否NTFS格式
	*/
	char sysNameBuf[MAX_PATH] = { 0 };
	status = GetVolumeInformationA(volName,
		NULL, // 驱动盘名缓冲，这里我们不需要
		0,
		NULL,
		NULL,
		NULL,
		sysNameBuf, // 驱动盘的系统名（FAT/NTFS）
		MAX_PATH);

	if (0 != status){
		printf("文件系统名: %s\n", sysNameBuf);

		// 比较字符串
		if (0 == strcmp(sysNameBuf, "NTFS")){
			isNTFS = true;
		}
		else{
			printf("该驱动盘非NTFS格式\n");
		}

	}
	
	// 只有NTFS才有USN，才能进行操作
	if (isNTFS){
		/**
		* step 02. 获取驱动盘句柄
		*/
		char fileName[MAX_PATH];
		fileName[0] = '\0';

		// 传入的文件名必须为\\.\C:的形式
		strcpy_s(fileName, "\\\\.\\");
		strcat_s(fileName, volName);
		// 为了方便操作，这里转为string进行去尾
		string fileNameStr = (string)fileName;
		fileNameStr.erase(fileNameStr.find_last_of(":") + 1);

		printf("驱动盘地址: %s\n", fileNameStr.data());

		// 调用该函数需要管理员权限
		hVol = CreateFileA(fileNameStr.data(),
			GENERIC_READ | GENERIC_WRITE, // 可以为0
			FILE_SHARE_READ | FILE_SHARE_WRITE, // 必须包含有FILE_SHARE_WRITE
			NULL, // 这里不需要
			OPEN_EXISTING, // 必须包含OPEN_EXISTING, CREATE_ALWAYS可能会导致错误
			FILE_ATTRIBUTE_READONLY, // FILE_ATTRIBUTE_NORMAL可能会导致错误
			NULL); // 这里不需要

		if (INVALID_HANDLE_VALUE != hVol){
			getHandleSuccess = true;
		}
		else{
			printf("获取驱动盘句柄失败 —— handle:%x error:%d\n", hVol, GetLastError());
		}

	}

	if (getHandleSuccess){

		/**
		* step 03. 初始化USN日志文件
		*/
		DWORD br;
		CREATE_USN_JOURNAL_DATA cujd;
		cujd.MaximumSize = 0; // 0表示使用默认值
		cujd.AllocationDelta = 0; // 0表示使用默认值
		status = DeviceIoControl(hVol,
			FSCTL_CREATE_USN_JOURNAL,
			&cujd,
			sizeof(cujd),
			NULL,
			0,
			&br,
			NULL);

		if (0 != status){
			initUsnJournalSuccess = true;
		}
		else{
			printf("初始化USN日志文件失败 —— status:%x error:%d\n", status, GetLastError());
		}

	}

	if (initUsnJournalSuccess){

		bool getBasicInfoSuccess = false;

		/**
		* step 04. 获取USN日志基本信息(用于后续操作)
		*/
		DWORD br;
		status = DeviceIoControl(hVol,
			FSCTL_QUERY_USN_JOURNAL,
			NULL,
			0,
			&UsnInfo,
			sizeof(USN_JOURNAL_DATA),
			&br,
			NULL);

		if (0 != status){
			getBasicInfoSuccess = true;
		}
		else{
			printf("获取USN日志基本信息失败 —— status:%x error:%d\n", status, GetLastError());
		}

		if (getBasicInfoSuccess){

			printf("UsnJournalID: %xI64\n", UsnInfo.UsnJournalID);
			printf("lowUsn: %xI64\n", UsnInfo.FirstUsn);
			printf("highUsn: %xI64\n", UsnInfo.NextUsn);

			/**
			* step 05. 枚举USN日志文件中的所有记录
			*/

			// from MSDN
			// On the first call, set the starting point, the StartFileReferenceNumber member of the MFT_ENUM_DATA structure, to (DWORDLONG)0. 
			// Each call to FSCTL_ENUM_USN_DATA retrieves the starting point for the subsequent call as the first entry in the output buffer.
			MFT_ENUM_DATA med;
			med.StartFileReferenceNumber = 0;
			med.LowUsn = 0;//UsnInfo.FirstUsn; 这里经测试发现，如果用FirstUsn有时候不正确，导致获取到不完整的数据，还是直接写0好.
			med.HighUsn = UsnInfo.NextUsn;

			CHAR buffer[BUF_LEN]; // 用于储存记录的缓冲,尽量足够地大
			DWORD usnDataSize;
			PUSN_RECORD UsnRecord;

			while (0 != DeviceIoControl(hVol,
				FSCTL_ENUM_USN_DATA,
				&med,
				sizeof(med),
				buffer,
				BUF_LEN,
				&usnDataSize,
				NULL))
			{

				DWORD dwRetBytes = usnDataSize - sizeof(USN);

				// 找到第一个USN记录
				// return a USN followed by zero or more change journal records, each in a USN_RECORD structure. 
				UsnRecord = (PUSN_RECORD)(((PCHAR)buffer) + sizeof(USN));

				printf(" ********************************** \n");
				while (dwRetBytes>0){

					// 打印获取到的信息
					const int strLen = UsnRecord->FileNameLength;
					char fileName[MAX_PATH] = { 0 };
					WideCharToMultiByte(CP_OEMCP, NULL, UsnRecord->FileName, strLen / 2, fileName, strLen, NULL, FALSE);
					printf("FileName: %s\n", fileName);
					// 下面两个file reference number可以用来获取文件的路径信息
					printf("FileReferenceNumber: %xI64\n", UsnRecord->FileReferenceNumber);
					printf("ParentFileReferenceNumber: %xI64\n", UsnRecord->ParentFileReferenceNumber);
					printf("\n");

					fout << "FileName:" << fileName << endl;
					fout << "frn:" << UsnRecord->FileReferenceNumber << endl;
					fout << "pfrn:" << UsnRecord->ParentFileReferenceNumber << endl;
					fout << endl;
					counter++;

					// 获取下一个记录
					DWORD recordLen = UsnRecord->RecordLength;
					dwRetBytes -= recordLen;
					UsnRecord = (PUSN_RECORD)(((PCHAR)UsnRecord) + recordLen);
				}

				//获取下一页数据，MTF大概是分多页来储存的吧？
				// The USN returned as the first item in the output buffer is the USN of the next record number to be retrieved. 
				// Use this value to continue reading records from the end boundary forward.
				med.StartFileReferenceNumber = *(USN *)&buffer;

			}

			printf("共%d个文件\n", counter);

			fout << "共" << counter << "个文件" << endl;
			fout << flush;
			fout.close();

		}

		/**
		* step 06. 删除USN日志文件(当然也可以不删除)
		*/
		DELETE_USN_JOURNAL_DATA dujd;
		dujd.UsnJournalID = UsnInfo.UsnJournalID;
		dujd.DeleteFlags = USN_DELETE_FLAG_DELETE;

		status = DeviceIoControl(hVol,
			FSCTL_DELETE_USN_JOURNAL,
			&dujd,
			sizeof(dujd),
			NULL,
			0,
			&br,
			NULL);

		if (0 != status){
			printf("成功删除USN日志文件!\n");
		}
		else{
			printf("删除USN日志文件失败 —— status:%x error:%d\n", status, GetLastError());
		}

	}

	// 最后释放一些资源
	if (getHandleSuccess){
		CloseHandle(hVol);
	}

	// 避免后台程序一闪而过
	getchar();
	//MessageBox(0, L"按确定退出", L"结束", MB_OK);

	return 0;
}