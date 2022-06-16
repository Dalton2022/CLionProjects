int CommuServerMessag(int answer_socket) {
    static __thread char recvBuffer[MAXBUFSIZE]={0};
    static __thread int recvpose;


    //是否需要定义接收计数器？

    //解析到的位置下标
    int position = 0;


    //解析到的data的缓存区
    unsigned char result[MAXBUFSIZE]={0};
    //result的长度
    int resultLength = 0;

    memset(recvBuffer + recvpose, 0,  MAXBUFSIZE - recvpose);

    //这里是防止错误数据过多，占据整个缓冲区
    if (recvpose >= (MAXBUFSIZE - 1))
        recvpose = 0;

    //使用poll()函数监视socket
    struct pollfd pollFd;
    memset(&pollFd, 0, sizeof(pollFd));
    pollFd.fd = answer_socket;
    pollFd.events = POLLIN;
    if (poll(&pollFd, 1, 1)) {
        if (pollFd.revents & POLLIN) {
            //是否需要计数器？
            //尽可能多地从socket中读取信息
            size_t recvLength = recv(pollFd.fd, recvBuffer + recvpose, MAXBUFSIZE - recvpose, MSG_DONTWAIT);
            if (recvLength > 0) {
                //如果未解析的数据还够一帧的长度，就继续进行数据的查找
                while ((position + STARTSIZE + HEADSIZE + EOF_SIZE) < recvpose + recvLength) {
                    //每次循环进行清空
                    memset(result, 0, MAXBUFSIZE);
                    //取出result，
                    resultLength =  recMsgFrame_v3(recvBuffer, (int) recvLength + recvpose, &position, result);

                    //正确接收时处理result;
                    if (resultLength > 0) {
                        DisplayArray(result, resultLength);
                    }
                }
                //处理缓存区消息不完整的情况,这里很有可能下标计算错误！！（已修正）
                if (position < recvpose + recvLength) {
                    printf("保存不完整data\n");
                    int bufPosition;
                    for (bufPosition = 0; position < recvpose + recvLength; bufPosition++) {
                        recvBuffer[bufPosition] = recvBuffer[position];
                        position++;
                    }
                    recvpose = bufPosition;
                    printf("recvpose:%d\n", recvpose);
                    memset(recvBuffer + bufPosition, 0, MAXBUFSIZE - bufPosition);
                } else {
                    recvpose = 0;
                    memset(recvBuffer, 0, MAXBUFSIZE);
                }
            } else {
                //有数据，但是接收数据失败
                return -1;
            }
        } else if(pollFd.revents & POLLHUP) {
            //这里表示收到了poll的反馈，但是收到的不是POLLIN
            printf("Remote socket closed\n");
            return -1;
        }
    } else {
        //这次没有检测到消息的处理
        return -1;
    }
    return 0;
}

int recMsgFrame_v3(unsigned char* recvBuf, int recvBufSize, int *position, unsigned char *result)
{
    //存储原先的初始位置，代码写的很差
    int oriPosition = *position;
    int data_len;
    unsigned char is_data_crc = 0;
    unsigned char data_crc = 0;
    int find_times;
    unsigned char recv_temp_buf[TEMP_BUF_SIZE] = {0};
    unsigned char head_temp_buf[HEADSIZE] = {0};
    //从传入的position位置开始遍历字符串recvBuf
    for (find_times = oriPosition; find_times < recvBufSize; find_times++) {
        if (*(recvBuf + find_times) != SOFBYTE) {
            (*position)++;
            continue;
        }
        //看看数据是否够一个头部的长度
        if (recvBufSize > find_times + HEADSIZE) {
            memcpy(head_temp_buf, recvBuf +find_times + STARTSIZE, HEADSIZE);
            if(head_temp_buf[HEADSIZE -1] == CalcCRC8_ChackTable(head_temp_buf, HEADSIZE-1))	//头部校验
            {//验证成功
                data_len = Bytes2Int ( head_temp_buf , DATALENSIZE);
                is_data_crc = head_temp_buf[DATALENSIZE];
                data_crc = head_temp_buf[DATALENSIZE + DATACHECKINDEX];
                break;
            } else {
                //验证失败
                memset(head_temp_buf, 0, HEADSIZE);
                printf("H_CRC ERROR!\n");
                (*position)++;
                continue;
            }
        } else {
            //数据不足一个头部
            printf("INCOMPLETE HEAD");
            return -1;
        }
    }
    //判断是否找到了帧头
    if (*position == recvBufSize) {
        printf("SOF FIND ERROR\n");
        return -1;
    }
    //判断数据长度是否合适
    if (data_len >= recvBufSize - *position - STARTSIZE - HEADSIZE - EOF_SIZE) {
        printf("INCOMPLETE DATA\n");
        return -1;
    }
    //获取数据
    memcpy(recv_temp_buf, recvBuf + *position + HEADSIZE + STARTSIZE, data_len + EOF_SIZE);
    //校验数据
    if (recv_temp_buf[data_len] != EOFBYTE || recv_temp_buf[data_len + 1] != EOFBYTE || recv_temp_buf[data_len + 2] != EOFBYTE || recv_temp_buf[data_len + 3] != EOFBYTE || recv_temp_buf[data_len + 4] != EOFBYTE) {
        printf("EOF ERROR\n");
        (*position)++;
        return -1;
    }
    if (is_data_crc) {
        if(data_crc != CalcCRC8_ChackTable(recv_temp_buf, data_len))
        {
            printf("DATA CRC ERROR!\n");
            (*position)++;
            return -1;															//数据校验出错
        }
    }
    (*position) += data_len + STARTSIZE + HEADSIZE + EOF_SIZE;
    //复制数据
    memcpy(result, recv_temp_buf, data_len);
    //DisplayArray(result, data_len);
    return data_len;
}