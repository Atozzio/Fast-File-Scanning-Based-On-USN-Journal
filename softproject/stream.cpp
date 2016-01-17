/**
* ��Ntfs��USN������ʾ������
*/
#include <iostream>
#include <Windows.h>
#include <fstream>
#include <string>
#include <time.h>

using namespace std;
char* volName = "F:\\";
HANDLE hVol;
USN_JOURNAL_DATA UsnInfo; // ����USN��־�Ļ�����Ϣ
#define BUF_LEN 4096

ofstream fout("E:\\log.txt");
long counter = 0;

int main()
{
	BOOL status;
	BOOL isNTFS = false;
	BOOL getHandleSuccess = false;
	BOOL initUsnJournalSuccess = false;

	//�ж��������Ƿ�NTFS��ʽ
	cout << "step 01. �ж��������Ƿ�NTFS��ʽ\n";
	char sysNameBuf[MAX_PATH] = { 0 };
	status = GetVolumeInformationA(volName,
		NULL,
		0,
		NULL,
		NULL,
		NULL,
		sysNameBuf, // �����̵�ϵͳ��
		MAX_PATH);
	cout << status << endl;
	if (0 != status){
		cout << "�ļ�ϵͳ��:" << sysNameBuf << "\n";
		// �Ƚ��ַ���
		if (0 == strcmp(sysNameBuf, "NTFS")){
			cout << "����������NTFS��ʽ��ת��step-02.\n";
			isNTFS = true;
		}
		else{
			cout << "�������̷�NTFS��ʽ\n";
		}

	}

	if (isNTFS){
		//step 02. ��ȡ�����̾��
		cout << "step 02. ��ȡ�����̾��\n";
		char fileName[MAX_PATH];
		fileName[0] = '\0';
		strcpy_s(fileName, "\\\\.\\");//������ļ���
		strcat_s(fileName, volName);
		string fileNameStr = (string)fileName;
		fileNameStr.erase(fileNameStr.find_last_of(":") + 1);
		cout << "�����̵�ַ:" << fileNameStr.data() << "\n";

		hVol = CreateFileA(fileNameStr.data(),//�ɴ򿪻򴴽����¶��󣬲����ؿɷ��ʵľ��������̨��ͨ����Դ��Ŀ¼��ֻ���򿪣����������������ļ�
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING, 
			FILE_ATTRIBUTE_READONLY, 
			NULL);
		cout << hVol << endl;

		if (INVALID_HANDLE_VALUE != hVol){
			cout << "��ȡ�����̾���ɹ���ת��step-03.\n";
			getHandleSuccess = true;
		}
		else{
			cout << "��ȡ�����̾��ʧ�� ���� handle:" << hVol << " error:" << GetLastError() << "\n";
		}
	}

	if (getHandleSuccess){
		//step 03. ��ʼ��USN��־�ļ�
		cout << "step 03. ��ʼ��USN��־�ļ�\n";
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
			cout << "��ʼ��USN��־�ļ��ɹ���ת��step-04.\n";
			initUsnJournalSuccess = true;
		}
		else{
			cout << "��ʼ��USN��־�ļ�ʧ�� ���� status:" << status << " error:" << GetLastError() << "\n";
		}
	}

	if (initUsnJournalSuccess){

		BOOL getBasicInfoSuccess = false;


		//step 04. ��ȡUSN��־������Ϣ(���ں�������)
		cout << "step 04. ��ȡUSN��־������Ϣ(���ں�������)\n";
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
			cout << "��ȡUSN��־������Ϣ�ɹ���ת��step-05.\n";
			getBasicInfoSuccess = true;
		}
		else{
			cout << "��ȡUSN��־������Ϣʧ�� ���� status:" << status << " error:" << GetLastError() << "\n";
		}
		if (getBasicInfoSuccess){
			cout << "UsnJournalID: " << UsnInfo.UsnJournalID << "\n";
			cout << "lowUsn: " << UsnInfo.FirstUsn << "\n";
			cout << "highUsn: " << UsnInfo.NextUsn << "\n";

			//step 05. ö��USN��־�ļ��е����м�¼
			cout << "step 05. ö��USN��־�ļ��е����м�¼\n";
			MFT_ENUM_DATA med;
			med.StartFileReferenceNumber = 0;
			med.LowUsn = 0;
			med.HighUsn = UsnInfo.NextUsn;

			CHAR buffer[BUF_LEN]; //�����¼�Ļ���,�����㹻�ش� buf_len = 4096
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

				UsnRecord = (PUSN_RECORD)(((PCHAR)buffer) + sizeof(USN));// �ҵ���һ��USN��¼
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

					// ��ȡ��һ����¼
					DWORD recordLen = UsnRecord->RecordLength;
					dwRetBytes -= recordLen;
					UsnRecord = (PUSN_RECORD)(((PCHAR)UsnRecord) + recordLen);
				}
				med.StartFileReferenceNumber = *(USN *)&buffer;

			}
			cout << "��" << counter << "���ļ�\n";
			long clock_end = clock();
			cout << "����" << clock_end - clock_start << "����" << endl;
			fout << "��" << counter << "���ļ�" << endl;
			fout << flush;
			fout.close();
		}


		//step 06. ɾ��USN��־�ļ�(��ȻҲ���Բ�ɾ��)
		cout << "step 06. ɾ��USN��־�ļ�(��ȻҲ���Բ�ɾ��)\n";
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
			cout << "�ɹ�ɾ��USN��־�ļ�!\n";
		}
		else{
			cout << "ɾ��USN��־�ļ�ʧ�� ���� status:" << status << " error:" << GetLastError() << "\n";
		}
	}
	if (getHandleSuccess){
		CloseHandle(hVol);
	}//�ͷ���Դ

	//MessageBox(0, L"��ȷ���˳�", L"����", MB_OK);
	getchar();
	return 0;
}