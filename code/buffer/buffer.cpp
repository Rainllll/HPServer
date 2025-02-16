#include "buffer.h"

// 读写下标初始化，vector<char>初始化
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

/// Buffer 内存布局:
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size


// 可写的数量：buffer大小 - 写下标
size_t Buffer::WritableBytes() const
{
    return buffer_.size() - writePos_;
}

// 可读的数量：写下标 - 读下标
size_t Buffer::ReadableBytes() const
{
    return writePos_ - readPos_;
}

// 可预留空间：已经读过的就没用了，等于读下标
size_t Buffer::PrependableBytes() const
{
    return readPos_;
}

/**
 * @brief 获取缓冲区中可读数据的起始指针
 * 
 * @return const char* 指向可读数据的起始位置的指针
 */
const char *Buffer::Peek() const
{
    // 返回缓冲区中读指针指向的位置，即可读数据的起始位置
    return &buffer_[readPos_];
}

// 确保可写的长度
/**
 * @brief 确保缓冲区有足够的可写空间
 * 
 * @param len 需要确保的可写空间大小
 */
void Buffer::EnsureWriteable(size_t len)
{
    // 如果需要的可写空间大于当前缓冲区的可写空间
    if (len > WritableBytes())
    {
        // 调用 MakeSpace_ 函数扩展缓冲区空间
        MakeSpace_(len);
    }
    // 断言确保扩展后的缓冲区有足够的可写空间
    assert(len <= WritableBytes());
}

// 移动写下标，在Append中使用
void Buffer::HasWritten(size_t len)
{
    writePos_ += len;
}

// 读取len长度，移动读下标
void Buffer::Retrieve(size_t len)
{
    readPos_ += len;
}

// 读取到end位置
void Buffer::RetrieveUntil(const char *end)
{
    assert(Peek() <= end);
    Retrieve(end - Peek()); // end指针 - 读指针 长度
}

// 取出所有数据，buffer归零，读写下标归零,在别的函数中会用到
void Buffer::RetrieveAll()
{
    memset(&buffer_[0], 0, buffer_.size()); // 覆盖原本数据
    readPos_ = writePos_ = 0;
}

// 取出剩余可读的str
std::string Buffer::RetrieveAllToStr()
{
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

// 写指针的位置
const char *Buffer::BeginWriteConst() const
{
    return &buffer_[writePos_];
}

char *Buffer::BeginWrite()
{
    return &buffer_[writePos_];
}

// 添加str到缓冲区
/**
 * @brief 将指定长度的字符数据追加到缓冲区
 * 
 * @param str 要追加的字符数据的指针
 * @param len 要追加的字符数据的长度
 */
void Buffer::Append(const char *str, size_t len)
{
    // 确保传入的字符指针不为空
    assert(str);
    // 确保缓冲区有足够的可写空间来容纳要追加的数据
    EnsureWriteable(len);
    // 将传入的字符数据复制到缓冲区的可写区域
    std::copy(str, str + len, BeginWrite());
    // 更新写指针，标记已写入的数据长度
    HasWritten(len);
}

/**
 * @brief 将字符串追加到缓冲区
 * 
 * @param str 要追加的字符串
 */
void Buffer::Append(const std::string &str)
{
    // 调用另一个重载的 Append 函数，将字符串的内容追加到缓冲区
    Append(str.c_str(), str.size());
}

/**
 * @brief 将指定长度的二进制数据追加到缓冲区
 * 
 * @param data 要追加的二进制数据的指针
 * @param len 要追加的二进制数据的长度
 */
void Buffer::Append(const void *data, size_t len)
{
    // 将传入的二进制数据指针转换为字符指针，并调用另一个重载的 Append 函数，将数据追加到缓冲区
    Append(static_cast<const char *>(data), len);
}

// 将buffer中的读下标的地方放到该buffer中的写下标位置
void Buffer::Append(const Buffer &buff)
{
    Append(buff.Peek(), buff.ReadableBytes());
}

// 将fd的内容读到缓冲区，即writable的位置
/**
 * @brief 从文件描述符中读取数据到缓冲区
 * 
 * @param fd 文件描述符
 * @param Errno 错误码指针
 * @return ssize_t 读取的字节数
 */
ssize_t Buffer::ReadFd(int fd, int *Errno)
{
    char buff[65535]; // 栈区
    struct iovec iov[2];
    size_t writeable = WritableBytes(); // 先记录能写多少
    // 分散读， 保证数据全部读完
    iov[0].iov_base = BeginWrite();
    iov[0].iov_len = writeable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    ssize_t len = readv(fd, iov, 2);
    if (len < 0)
    {
        *Errno = errno;
    }
    else if (static_cast<size_t>(len) <= writeable)
    {                     // 若len小于writable，说明写区可以容纳len
        writePos_ += len; // 直接移动写下标
    }
    else
    {
        writePos_ = buffer_.size();                         // 写区写满了,下标移到最后
        Append(buff, static_cast<size_t>(len - writeable)); // 剩余的长度
    }
    return len;
}

// 将buffer中可读的区域写入fd中
ssize_t Buffer::WriteFd(int fd, int *Errno)
{
    ssize_t len = write(fd, Peek(), ReadableBytes());
    if (len < 0)
    {
        *Errno = errno;
        return len;
    }
    Retrieve(len);
    return len;
}

char *Buffer::BeginPtr_()
{
    return &buffer_[0];
}

const char *Buffer::BeginPtr_() const
{
    return &buffer_[0];
}

// 扩展空间
/**
 * @brief 调整缓冲区空间以确保有足够的可写空间
 * 
 * @param len 需要确保的可写空间大小
 */
void Buffer::MakeSpace_(size_t len)
{
    // 如果当前可写空间加上预留空间（已读空间）仍小于所需空间
    if (WritableBytes() + PrependableBytes() < len)
    {
        // 直接扩展缓冲区大小，确保有足够的空间
        buffer_.resize(writePos_ + len + 1);
    }
    else
    {
        // 计算当前可读数据的长度
        size_t readable = ReadableBytes();
        // 将可读数据移动到缓冲区的起始位置
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        // 重置读指针为0
        readPos_ = 0;
        // 重置写指针为可读数据的长度
        writePos_ = readable;
        // 断言确保移动后可读数据的长度不变
        assert(readable == ReadableBytes());
    }
}
