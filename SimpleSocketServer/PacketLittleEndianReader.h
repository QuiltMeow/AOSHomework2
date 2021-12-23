#pragma once

#include <iomanip>
#include <fstream>
#include <sstream>
#include <string.h>

class PacketLittleEndianReader {
private:
    unsigned char* data;
    int length, current;
protected:
public:
    PacketLittleEndianReader(unsigned char* input, int length) {
        data = new unsigned char[length];
        for (int i = 0; i < length; ++i) {
            data[i] = input[i];
        }
        this->length = length;
        current = 0;
    }

    PacketLittleEndianReader(PacketLittleEndianReader& copy) {
        length = copy.length;
        data = new unsigned char[length];
        for (int i = 0; i < length; ++i) {
            data[i] = copy.data[i];
        }
        current = copy.current;
    }

    virtual ~PacketLittleEndianReader() {
        delete[] data;
    }

    unsigned char readByte() {
        if (current + 1 > length) {
            throw std::runtime_error("嘗試存取超出陣列範圍元素");
        }
        return data[current++] & 0xFF;
    }

    unsigned short readUShort() {
        short read = readShort();
        if (read < 0) {
            read += 65536;
        }
        return (unsigned short)read;
    }

    short readShort() {
        unsigned char byte[2];
        for (int i = 0; i < 2; ++i) {
            byte[i] = readByte();
        }
        return ((short)byte[1] << (short)8) + (short)byte[0];
    }

    int readInt() {
        unsigned char byte[4];
        for (int i = 0; i < 4; ++i) {
            byte[i] = readByte();
        }
        return ((int)byte[3] << (int)24) + ((int)byte[2] << (int)16) + ((int)byte[1] << (int)8) + (int)byte[0];
    }

    float readFloat() {
        float ret;
        unsigned char byte[4];
        for (int i = 0; i < 4; ++i) {
            byte[i] = readByte();
        }
        memcpy(&ret, byte, 4);
        return ret;
    }

    long long int readLong() {
        unsigned char byte[8];
        for (int i = 0; i < 8; ++i) {
            byte[i] = readByte();
        }
        return ((long long int) byte[7] << (long long int) 56) + ((long long int) byte[6] << (long long int) 48) + ((long long int) byte[5] << (long long int) 40) + ((long long int) byte[4] << (long long int) 32) + ((long long int) byte[3] << (long long int) 24) + ((long long int) byte[2] << (long long int) 16) + ((long long int) byte[1] << (long long int) 8) + (long long int) byte[0];
    }

    double readDouble() {
        double ret;
        unsigned char byte[8];
        for (int i = 0; i < 8; ++i) {
            byte[i] = readByte();
        }
        memcpy(&ret, byte, 8);
        return ret;
    }

    std::string readAsciiString(int number) {
        std::stringstream ss;
        for (int i = 0; i < number; ++i) {
            ss << readByte();
        }
        return ss.str();
    }

    std::string readLengthAsciiString() {
        return readAsciiString((int)readShort());
    }

    std::pair<short, short> readPos() {
        short x = readShort();
        short y = readShort();
        return std::pair<short, short>(x, y);
    }

    unsigned char* read(int number) {
        int check = current + number;
        if (check > length) {
            throw std::runtime_error("嘗試存取超出陣列範圍元素");
        }
        unsigned char* ret = new unsigned char[number];
        for (int i = 0; i < number; ++i) {
            ret[i] = readByte();
        }
        return ret;
    }

    std::pair<unsigned char*, int> readFile() {
        int size = readInt();
        if (size == -1) {
            return std::pair<unsigned char*, int>(NULL, -1);
        }
        unsigned char* file = read(size);
        return std::pair<unsigned char*, int>(file, size);
    }

    void unReadByte() {
        if (current - 1 < 0) {
            return;
        }
        --current;
    }

    void unReadShort() {
        if (current - 2 < 0) {
            current = 0;
            return;
        }
        current -= 2;
    }

    void unReadInt() {
        if (current - 4 < 0) {
            current = 0;
            return;
        }
        current -= 4;
    }

    void unReadLong() {
        if (current - 8 < 0) {
            current = 0;
            return;
        }
        current -= 8;
    }

    void unReadAsciiString(int number) {
        unRead(number);
    }

    void unReadPos() {
        unReadInt();
    }

    void unRead(int number) {
        if (current - number < 0) {
            current = 0;
            return;
        }
        current -= number;
    }

    unsigned char readLastByte() {
        return data[current - 1] & 0xFF;
    }

    short readLastShort() {
        unReadShort();
        return readShort();
    }

    int readLastInt() {
        unReadInt();
        return readInt();
    }

    long long int readLastLong() {
        unReadLong();
        return readLong();
    }

    std::string readLastAsciiString(int number) {
        unRead(number);
        return readAsciiString(number);
    }

    std::pair<short, short> readLastPos() {
        unReadInt();
        short x = readShort();
        short y = readShort();
        return std::pair<short, short>(x, y);
    }

    unsigned char* readLastByte(int number) {
        if (current - number < 0) {
            throw std::runtime_error("超出封包頭部範圍");
        }
        unRead(number);
        return read(number);
    }

    void skip(int number) {
        if (number < 0) {
            return;
        } else if (current + number > length) {
            current = length;
            return;
        }
        current += number;
    }

    void seek(int value) {
        if (value < 0 || value > length) {
            return;
        }
        current = value;
    }

    int available() {
        return length - current;
    }

    int getByteRead() {
        return current;
    }

    int size() {
        return length;
    }

    std::string toString() {
        std::stringstream now;
        std::stringstream all;
        if (length - current > 0) {
            for (int i = current; i < length; ++i) {
                now << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << (short)data[i];
                if (i < length - 1) {
                    now << " ";
                }
            }
        }
        for (int i = 0; i < length; ++i) {
            all << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << (short)data[i];
            if (i < length - 1) {
                all << " ";
            }
        }
        std::stringstream ret;
        ret << "全部 : " << all.str();
        if (now.str() != "") {
            ret << std::endl << "目前 : " << now.str();
        }
        return ret.str();
    }
};
