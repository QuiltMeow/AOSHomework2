#pragma once

#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>

class PacketLittleEndianWriter {
private:
    int defaultLength;
    int offset;
    int length, current;
    unsigned char* data;

    void checkOverflow(int nextSize) {
        while (true) {
            if (current + nextSize >= length) {
                length *= 2;
                unsigned char* replace = new unsigned char[length];
                for (int i = 0; i < current; ++i) {
                    replace[i] = data[i];
                }
                delete[] data;
                data = replace;
                continue;
            }
            return;
        }
    }

    void write(unsigned char* value, int length) {
        checkOverflow(length);
        for (int i = 0; i < length; ++i) {
            data[current++] = value[i];
        }
    }

    void initBuffer(int length = 32) {
        defaultLength = this->length = length;
        current = 0;
        data = new unsigned char[length];
    }
protected:
public:
    PacketLittleEndianWriter() {
        initBuffer();
        offset = rand() % 11 * (rand() % 2 == 1 ? -1 : 1);
    }

    PacketLittleEndianWriter(PacketLittleEndianWriter& copy) {
        defaultLength = copy.defaultLength;
        length = copy.length;
        current = copy.current;
        data = new unsigned char[length];
        for (int i = 0; i < current; ++i) {
            data[i] = copy.data[i];
        }
        offset = copy.offset;
    }

    PacketLittleEndianWriter(int length) {
        initBuffer(length);
        offset = rand() % 11 * (rand() % 2 == 1 ? -1 : 1);
    }

    virtual ~PacketLittleEndianWriter() {
        delete[] data;
    }

    void clear() {
        delete[] data;
        initBuffer(defaultLength);
    }

    void writeZero() {
        write((char)0);
    }

    void write(bool value) {
        checkOverflow(1);
        data[current++] = (unsigned char)value;
    }

    void write(char value) {
        checkOverflow(1);
        data[current++] = (unsigned char)value;
    }

    void write(const char* value, int length) {
        checkOverflow(length);
        for (int i = 0; i < length; ++i) {
            data[current++] = (unsigned char)value[i];
        }
    }

    void writeBool(bool value) {
        write(value);
    }

    void writeByte(char value) {
        write(value);
    }

    void writeShort(short value) {
        write((char)(value & 0xFF));
        write((char)(((unsigned short)value >> 8) & 0xFF));
    }

    void writeInt(int value) {
        write((char)(value & 0xFF));
        write((char)(((unsigned int)value >> 8) & 0xFF));
        write((char)(((unsigned int)value >> 16) & 0xFF));
        write((char)(((unsigned int)value >> 24) & 0xFF));
    }

    void writeFloat(float value) {
        unsigned char byte[4];
        memcpy(byte, (unsigned char*)(&value), 4);
        write(byte, 4);
    }

    void writeLong(long long int value) {
        write((char)(value & 0xFF));
        write((char)(((unsigned long long int) value >> 8) & 0xFF));
        write((char)(((unsigned long long int) value >> 16) & 0xFF));
        write((char)(((unsigned long long int) value >> 24) & 0xFF));
        write((char)(((unsigned long long int) value >> 32) & 0xFF));
        write((char)(((unsigned long long int) value >> 40) & 0xFF));
        write((char)(((unsigned long long int) value >> 48) & 0xFF));
        write((char)(((unsigned long long int) value >> 56) & 0xFF));
    }

    void writeDouble(double value) {
        unsigned char byte[8];
        memcpy(byte, (unsigned char*)(&value), 8);
        write(byte, 8);
    }

    void writeAsciiString(std::string value) {
        write(value.c_str(), value.length());
    }

    void writeLengthAsciiString(std::string value) {
        writeShort((short)value.length());
        writeAsciiString(value);
    }

    void writePos(int x, int y) {
        writeShort((short)x);
        writeShort((short)y);
    }

    void writePosRx(int x, int y) {
        writeShort((short)(x + 50));
        writeShort((short)y);
    }

    void writePosRxRandom(int x, int y) {
        writeShort((short)(x + offset));
        writeShort((short)y);
    }

    void writeSame(char value, int length) {
        for (int i = 0; i < length; ++i) {
            writeByte(value);
        }
    }

    void writeZeroByte(int length) {
        writeSame((char)0, length);
    }

    void writeFile(std::string path) {
        std::fstream fs;
        fs.open(path, std::ios::in | std::ios::binary);
        if (!fs) {
            writeInt(-1);
            return;
        }
        fs.seekg(0, std::ios::end);
        int size = fs.tellg();
        writeInt(size);

        if (size > 0) {
            fs.seekg(0, std::ios::beg);
            std::vector<char> buffer(std::istreambuf_iterator<char>(fs), (std::istreambuf_iterator<char>()));
            char* fileByte = &buffer[0];
            write(fileByte, buffer.size());
        }
        fs.close();
    }

    void skip(int length) {
        for (int i = 0; i < length; ++i) {
            writeByte((char)(rand() % 255 + 1));
        }
    }

    int size() {
        return current;
    }

    unsigned char* getPacket() {
        unsigned char* ret = new unsigned char[current];
        for (int i = 0; i < current; ++i) {
            ret[i] = data[i];
        }
        return ret;
    }

    void copyPacket(unsigned char* address) {
        copyPacket(address, current);
    }

    void copyPacket(unsigned char* address, int copyLength) {
        int size = copyLength > current ? current : copyLength;
        for (int i = 0; i < size; ++i) {
            address[i] = data[i];
        }
    }

    std::string toString() {
        std::stringstream ss;
        for (int i = 0; i < current; ++i) {
            ss << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << (short)data[i];
            if (i < current - 1) {
                ss << " ";
            }
        }
        return ss.str();
    }
};
