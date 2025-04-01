#include <bitset>
#include <iostream>
#include <vector>
#include <any>
#include <string>
#include <fstream>
#include <cstdint>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <functional>
#include <iomanip>

namespace BitTorrent {
    class BEncoding {
        static constexpr uint8_t DictionaryStart  = 'd';
        static constexpr uint8_t DictionaryEnd    = 'e';
        static constexpr uint8_t ListStart        = 'l';
        static constexpr uint8_t ListEnd          = 'e';
        static constexpr uint8_t NumberStart      = 'i';
        static constexpr uint8_t NumberEnd        = 'e';
        static constexpr uint8_t ByteArrayDivider = ':';

    public:
        std::any Decode(std::vector<uint8_t>& bytes) {
            auto iter = bytes.begin();
            return DecodeNextObject(iter);
        };

        std::any DecodeFile(const std::string& path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) {
                throw std::runtime_error("Unable to find file: " + path);
            }
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<uint8_t> bytes(size);
            if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) {
                throw std::runtime_error("Unable to read file: " + path);
            }

            return BEncoding().Decode(bytes);
        }

        std::vector<uint8_t> Encode(std::any obj){
            std::stringstream buffer; //creating a stream buffer
            EncodeNextObject(buffer, obj);
            std::string str = buffer.str(); // extract string from the stream buffer to str var
            return std::vector<uint8_t>(str.begin(), str.end()); //returning the vector of bytes
        }

        void EncodeToFile(std::any obj, const std::string& path) {
            std::vector<uint8_t> bytes = Encode(obj);
            std::ofstream file(path, std::ios::binary); //open the file in binary
            file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()); //write the bytes to the file
        }


    private:
        void EncodeNextObject(std::stringstream& buffer, std::any obj) {
            if (obj.type() == typeid(std::vector<uint8_t>)) {
                EncodeByteArray(buffer, std::any_cast<const std::vector<uint8_t>&>(obj));
            }
            else if (obj.type() == typeid(std::string)) {
                EncodeString(buffer, std::any_cast<std::string&>(obj));
            }
            else if (obj.type() == typeid(long)){
                EncodeNumber(buffer, std::any_cast<long>(obj));
            }
            else if (obj.type() == typeid(std::vector<std::any>)) {
                EncodeList(buffer, std::any_cast<std::vector<std::any>&>(obj));
            }
            else if (obj.type() == typeid(std::unordered_map<std::string, std::any>)) {
                EncodeDictionary(buffer, std::any_cast<std::unordered_map<std::string, std::any>>(obj));
            }
            else {
                throw std::runtime_error("Invalid object type");
            }
        } 
        void EncodeNumber(std::stringstream& buffer, long input) {
            buffer << NumberStart; 
            std::string numStr = std::to_string(input);
            buffer.write(numStr.c_str(), numStr.size());
            buffer << NumberEnd;
        }
        void EncodeByteArray(std::stringstream& buffer, const std::vector<uint8_t>& body){
            std::string length = std::to_string(body.size());
            buffer.write(length.c_str(), length.size());
            buffer << ByteArrayDivider;
            buffer.write(reinterpret_cast<const char*>(body.data()), body.size());
        }
        void EncodeString(std::stringstream& buffer, const std::string& input){
            std::vector<uint8_t> byteArray(input.begin(), input.end());
            EncodeByteArray(buffer, byteArray);
        }
        void EncodeList(std::stringstream& buffer, std::vector<std::any>& input){
            buffer << ListStart;
            for (auto& item : input){
                EncodeNextObject(buffer, item);
            }
            buffer << ListEnd;
        }
        void EncodeDictionary(std::stringstream& buffer, const std::unordered_map<std::string, std::any>& input){
            buffer << DictionaryStart;
            std::vector<std::string> keys;
            for (auto& pair : input){
                keys.push_back(pair.first);
            }
            sort(keys.begin(), keys.end());
            for (auto& key : keys){
                EncodeString(buffer, key);
                EncodeNextObject(buffer, input.at(key));
            }
            buffer << DictionaryEnd;
        }
        std::any DecodeNextObject(std::vector<uint8_t>::iterator& iter) {
            if (*iter == DictionaryStart) {
                ++iter; // Move past 'd'
                return DecodeDictionary(iter);
            } else if (*iter == ListStart) {
                ++iter; // Move past 'l'
                return DecodeList(iter);
            } else if (*iter == NumberStart) {
                return DecodeNumber(iter);
            } else {
                return DecodeByteArray(iter);
            }
        }

        long DecodeNumber(std::vector<uint8_t>::iterator& iter) {
            if (*iter != NumberStart) {
                throw std::runtime_error("Invalid number format");
            }
            ++iter; // Skip 'i'

            std::vector<uint8_t> bytes;
            while (*iter != NumberEnd) {
                bytes.push_back(*iter);
                ++iter;
            }
            ++iter; // Skip 'e'

            std::string numAsString(bytes.begin(), bytes.end());
            return std::stoll(numAsString);
        }

        bool TryParseInt(const std::string& str, int& result) {
            try {
                result = std::stoi(str);
                return true;    
            } catch (const std::exception&) {
                result = 0;
                return false;
            }
        }

        std::vector<uint8_t> DecodeByteArray(std::vector<uint8_t>::iterator& iter) {
            std::vector<uint8_t> lengthBytes;
            while (*iter != ByteArrayDivider) {
                lengthBytes.push_back(*iter);
                ++iter;
            }
            ++iter; // Skip ':'

            std::string lengthString(lengthBytes.begin(), lengthBytes.end());

            int length;
            if (!TryParseInt(lengthString, length)) {
                throw std::runtime_error("Invalid byte array length");
            }

            std::vector<uint8_t> bytes;
            for (int i = 0; i < length; ++i) {
                bytes.push_back(*iter);
                ++iter;
            }
            return bytes;
        }

        std::vector<std::any> DecodeList(std::vector<uint8_t>::iterator& iter) {
            std::vector<std::any> list;
            while (*iter != ListEnd) {
                list.push_back(DecodeNextObject(iter));
            }
            ++iter; // Move past 'e'
            return list;
        }

        std::unordered_map<std::string, std::any> DecodeDictionary(std::vector<uint8_t>::iterator& iter) {
            std::unordered_map<std::string, std::any> dict;
            std::vector<std::string> keys;

            while (*iter != DictionaryEnd) {
                std::vector<uint8_t> keyBytes = DecodeByteArray(iter);
                std::string key(keyBytes.begin(), keyBytes.end());

                std::any val = DecodeNextObject(iter);
                keys.push_back(key);
                dict.insert({key, val});
            }
            ++iter; // Move past 'e'

            std::vector<std::string> sortedKeys = keys;
            std::sort(sortedKeys.begin(), sortedKeys.end());

            if (keys != sortedKeys) {
                throw std::runtime_error("error loading dictionary: keys not sorted");
            }
            return dict;
        }
    };
    class MemoryStreamExtensions{
        public:
            static void Append(std::stringstream& stream, uint8_t value){
                stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
            }
            static void Append(std::stringstream& stream, std::vector<uint8_t>& values){
                stream.write(reinterpret_cast<const char*>(values.data()), values.size());
            }
    };
}
