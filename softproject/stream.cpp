/**
* 对Ntfs下USN操作的示例程序
*/
#include <iostream>
#include <Windows.h>
#include <fstream>
#include <string>
#include <time.h>

using namespace std;
char* volName = "F:\\";
HANDLE hVol;
USN_JOURNAL_DATA UsnInfo; // 储存USN日志的基本信息
#define BUF_LEN 4096

ofstream fout("E:\\log.txt");
long counter = 0;

int main()
{
	BOOL status;
	BOOL isNTFS = false;
	BOOL getHandleSuccess = false;
	BOOL initUsnJournalSuccess = false;

	//判断驱动盘是否NTFS格式
	cout << "step 01. 判断驱动盘是否NTFS格式\n";
	char sysNameBuf[MAX_PATH] = { 0 };
	status = GetVolumeInformationA(volName,
		NULL,
		0,
		NULL,
		NULL,
		NULL,
		sysNameBuf, // 驱动盘的系统名
		MAX_PATH);
	cout << status << endl;
	if (0 != status){
		cout << "文件系统名:" << sysNameBuf << "\n";
		// 比较字符串
		if (0 == strcmp(sysNameBuf, "NTFS")){
			cout << "此驱动盘是NTFS格式！转向step-02.\n";
			isNTFS = true;
		}
		else{
			cout << "该驱动盘非NTFS格式\n";
		}

	}

	if (isNTFS){
		//step 02. 获取驱动盘句柄
		cout << "step 02. 获取驱动盘句柄\n";
		char fileName[MAX_PATH];
		fileName[0] = '\0';
		strcpy_s(fileName, "\\\\.\\");//传入的文件名
		strcat_s(fileName, volName);
		string fileNameStr = (string)fileName;
		fileNameStr.erase(fileNameStr.find_last_of(":") + 1);
		cout << "驱动盘地址:" << fileNameStr.data() << "\n";

		hVol = CreateFileA(fileNameStr.data(),//可打开或创建以下对象，并返回可访问的句柄：控制台，通信资源，目录（只读打开），磁盘驱动器，文件
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING, 
			FILE_ATTRIBUTE_READONLY, 
			NULL);
		cout << hVol << endl;

		if (INVALID_HANDLE_VALUE != hVol){
			cout << "获取驱动盘句柄成功！转向step-03.\n";
			getHandleSuccess = true;
		}
		else{
			cout << "获取驱动盘句柄失败 —— handle:" << hVol << " error:" << GetLastError() << "\n";
		}
	}

	if (getHandleSuccess){
		//step 03. 初始化USN日志文件
		cout << "step 03. 初始化USN日志文件\n";
		DWORD br;
		CREATE_USN_JOURNAL_DATA cujd;
		cujd.MaximumSize = 0;
		cujd.AllocationDelta = 0;
		status = DeviceIoControl(hVol,
			FSCTL_CREATE_USN_JOURNAL,
			&cujd,
			sizeof(cujd),
			NULL,
			0,
			&br,
			NULL);

		if (0 != status){
			cout << "初始化USN日志文件成功！转向step-04.\n";
			initUsnJournalSuccess = true;
		}
		else{
			cout << "初始化USN日志文件失败 —— status:" << status << " error:" << GetLastError() << "\n";
		}
	}

	if (initUsnJournalSuccess){

		BOOL getBasicInfoSuccess = false;


		//step 04. 获取USN日志基本信息(用于后续操作)
		cout << "step 04. 获取USN日志基本信息(用于后续操作)\n";
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
			cout << "获取USN日志基本信息成功！转向step-05.\n";
			getBasicInfoSuccess = true;
		}
		else{
			cout << "获取USN日志基本信息失败 —— status:" << status << " error:" << GetLastError() << "\n";
		}
		if (getBasicInfoSuccess){
			cout << "UsnJournalID: " << UsnInfo.UsnJournalID << "\n";
			cout << "lowUsn: " << UsnInfo.FirstUsn << "\n";
			cout << "highUsn: " << UsnInfo.NextUsn << "\n";

			//step 05. 枚举USN日志文件中的所有记录
			cout << "step 05. 枚举USN日志文件中的所有记录\n";
			MFT_ENUM_DATA med;
			med.StartFileReferenceNumber = 0;
			med.LowUsn = 0;
			med.HighUsn = UsnInfo.NextUsn;

			CHAR buffer[BUF_LEN]; //储存记录的缓冲,尽量足够地大 buf_len = 4096
			DWORD usnDataSize;
			PUSN_RECORD UsnRecord;
			long clock_start = clock();
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

				UsnRecord = (PUSN_RECORD)(((PCHAR)buffer) + sizeof(USN));// 找到第一个USN记录
				while (dwRetBytes>0){
					const int strLen = UsnRecord->FileNameLength;
					char fileName[MAX_PATH] = { 0 };
					//char filePath[MAX_PATH] = {0};
					WideCharToMultiByte(CP_OEMCP, NULL, UsnRecord->FileName, strLen / 2, fileName, strLen, NULL, FALSE);

					//cout << "FileName: " << fileName << "\n";
					//cout << "FileReferenceNumber: " << UsnRecord->FileReferenceNumber << "\n";
					//cout << "ParentFileReferenceNumber: " << UsnRecord->ParentFileReferenceNumber << "\n";
					////cout<< "FilePath: " << filePath << "\n\n";

					fout << "FileName:" << fileName << endl;
					fout << "FileReferenceNumber:" << UsnRecord->FileReferenceNumber << endl;
					fout << "ParentFileReferenceNumber:" << UsnRecord->ParentFileReferenceNumber << endl;
					//fout << "FilePath:" << filePath << endl;
					fout << endl;
					counter++;

					// 获取下一个记录
					DWORD recordLen = UsnRecord->RecordLength;
					dwRetBytes -= recordLen;
					UsnRecord = (PUSN_RECORD)(((PCHAR)UsnRecord) + recordLen);
				}
				med.StartFileReferenceNumber = *(USN *)&buffer;

			}
			cout << "共" << counter << "个文件\n";
			long clock_end = clock();
			cout << "花费" << clock_end - clock_start << "毫秒" << endl;
			fout << "共" << counter << "个文件" << endl;
			fout << flush;
			fout.close();
		}


		//step 06. 删除USN日志文件(当然也可以不删除)
		cout << "step 06. 删除USN日志文件(当然也可以不删除)\n";
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
			cout << "成功删除USN日志文件!\n";
		}
		else{
			cout << "删除USN日志文件失败 —— status:" << status << " error:" << GetLastError() << "\n";
		}
	}
	if (getHandleSuccess){
		CloseHandle(hVol);
	}//释放资源

	//MessageBox(0, L"按确定退出", L"结束", MB_OK);
	getchar();
	return 0;
}