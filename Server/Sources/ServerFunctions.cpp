#include "ServerFunctions.h"

string fileName(string pathname)
{
    const size_t last_slash_idx = pathname.find_last_of("\\/");
    if (string::npos != last_slash_idx)
    {
        pathname.erase(0, last_slash_idx + 1);
    }
    return (pathname);
}

unsigned appFileData(string folderAndFileName, const char *data, unsigned len)
{
    ofstream    fout;
    string      filePath = SAVE_PATH + folderAndFileName;

    fout.open(filePath, ios_base::app);
    if (!fout.is_open())
    {
        cerr << "ERROR. Файл " << filePath << " для записи не открыт!\n";
        return (0);
    }
    fout.write(data, len);
    fout.close();
    return (len);
}

int recvAll(int sock, char *buf, TYPE &dataType, list<string> *paths)
{
    int         readBytes;
    unsigned    totalBytes;
    unsigned    blockBytes;
    unsigned    curByte;
    unsigned    writeBytes;
    string      filename;
    MainHeader  mainHeader;
    FileHeader  fileHeader;
    string      folderName = "Files/"; //В эту строку нужно присвоить имя папки для сохранения файлов
                                    //не путь, а имя папки, которое заканчивается слэшем!!!
                                    //сам путь содержится в h-файле в SAVE_PATH

    paths->clear();
    totalBytes = 0;
    while (totalBytes < sizeof(MainHeader))
    {
        readBytes = recv(sock, buf + totalBytes, BUF_SIZE - totalBytes, 0);
        if (readBytes <= 0)
        {
            cerr << "Error: recvAll() readBytes <= 0" << endl;
            return (readBytes);
        }
        totalBytes += (int)readBytes;
    }
    
    mainHeader.setByteArr(buf);

    curByte = sizeof(MainHeader);
    blockBytes = totalBytes;
    cerr << mainHeader.getType() << "==" << SEND_FILES << endl;
    if (mainHeader.getType() == SEND_FILES)
    {
        dataType = SEND_FILES;
        for (int i = 0; i < mainHeader.getCount(); i++)
        {
            while (blockBytes - curByte < sizeof(FileHeader))
            {
                memcpy(buf, buf + curByte, blockBytes - curByte);
                blockBytes = blockBytes - curByte;
                curByte = 0;
                readBytes = recv(sock, buf + blockBytes, BUF_SIZE - blockBytes, 0);
                if (readBytes <= 0)
                {
                    cerr << "Error: recvAll() readBytes <= 0" << endl;
                    return (readBytes);
                }
                totalBytes += readBytes;
                blockBytes += readBytes;
            }
            fileHeader.setByteArr(buf + curByte);
            paths->push_back(string(SAVE_PATH) + folderName + string(fileHeader.getFileName()));
            curByte += sizeof(FileHeader);
            writeBytes = 0;
            while (writeBytes != fileHeader.getFileSize())
            {
                if ((curByte == blockBytes))
                {
                    curByte = 0;
                    blockBytes = 0;

                    readBytes = recv(sock, buf, BUF_SIZE, 0);
                    if (readBytes <= 0)
                    {
                        cerr << "Error: recvAll() readBytes <= 0" << endl;
                        return (readBytes);
                    }
                    totalBytes += (int)readBytes;
                    blockBytes = (int)readBytes;
                }
                if ((blockBytes - curByte) <= (fileHeader.getFileSize() - writeBytes))
                {
                    writeBytes += appFileData(folderName + string(fileHeader.getFileName()), 
                                                buf + curByte, blockBytes - curByte);
                    curByte += blockBytes - curByte;
                }
                else
                {
                    int new_writeBytes;
                    new_writeBytes = fileHeader.getFileSize() - writeBytes;
                    writeBytes += appFileData(folderName + string(fileHeader.getFileName()), 
                                                buf + curByte, new_writeBytes);
                    curByte += new_writeBytes;
                }
            }
        }
    }

    if (mainHeader.getType() == OTHER)
    {
        dataType = OTHER;
        int a = 2 * 2;
    }

    return (totalBytes);
}

unsigned getFileSize(string path)
{
    ifstream    fin;
    streampos   begin, end;

    fin.open(path, ios_base::binary);
    if (!fin.is_open())
    {
        cout << "fileSize: file " << path << " wasn't open. Errno: " << errno << endl;
        return (0);
    }
    begin = fin.tellg();
    fin.seekg(0, ios::end);
    end = fin.tellg();
    return ((unsigned)(end - begin));
}

unsigned getMessageSize(list<string> &paths)
{
    unsigned msgSize;
    unsigned fileSize;

    msgSize = sizeof(MainHeader);
    for(string filePath : paths)
    {
        msgSize += (unsigned)sizeof(FileHeader);
        fileSize = getFileSize(filePath);
        if (fileSize == 0)
            return (0);
        msgSize += fileSize;
    }
    return (msgSize);
}

int sendAll(int sock_fd, char *buf, int len, int flags)
{
    int total = 0;
    int n;

    while(total < len)
    {
        n = send(sock_fd, buf+total, len-total, flags);
        if(n == -1) { break; }
        total += n;
    }
    return (n==-1 ? -1 : total);
}

int sendData(int sock, const MainHeader &mainHdr, char* data, unsigned len)
{
    int totalBytes;
    int sendBytes;

    sendBytes = sendAll(sock, (char*)&mainHdr, sizeof(MainHeader), 0);
    if (sendBytes <= 0)
        return (sendBytes);
    totalBytes += sendBytes;

    sendBytes = sendAll(sock, data, len, 0);
    if (sendBytes <= 0)
        return (sendBytes);
    totalBytes += sendBytes;
    return (totalBytes);
}

int sendFiles(int sock, list<string> &paths)
{
    char        buf[BUF_SIZE];
    ifstream    fin;
    streampos   begin, end;
    MainHeader  mainHeader;
    FileHeader  fileHeader;
    unsigned    readBytes;
    int         sendBytes;
    int         totalBytes;  

    totalBytes = 0;
    mainHeader.setData(getMessageSize(paths), paths.size(), SEND_FILES);
    sendBytes = sendAll(sock, (char*)&mainHeader, sizeof(MainHeader), 0);
    if (sendBytes <= 0)
        return (sendBytes);
    totalBytes += sendBytes;
    for(string filePath : paths)
    {
        fin.open(filePath);
        if (!fin.is_open())
        {
            cout << "sendSolution: file " << filePath << " wasn't open. Errno: " << errno << endl;
            return (0);
        }
        else
            cout << "File " << filePath << " was opened for sending!" << endl;

        fileHeader.setData(getFileSize(filePath), (char*)fileName(filePath).data());
        sendBytes = sendAll(sock, (char*)&fileHeader, sizeof(FileHeader), 0);
        if (sendBytes <= 0)
            return (sendBytes);
        totalBytes += sendBytes;
        for (;;)
        {
            readBytes = (unsigned)fin.readsome(buf, (unsigned)BUF_SIZE);
            if (!readBytes)
                break;            
            sendBytes = sendAll(sock, buf, readBytes, 0);
            if (sendBytes <= 0)
                return (sendBytes);
            totalBytes += sendBytes;
        }
        fin.close();
    }
    return (totalBytes);
}