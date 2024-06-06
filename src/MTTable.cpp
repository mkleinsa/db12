#include <Rcpp.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <filesystem>
#include "MTDataFrame.h"
#include "MTSubTable.h"
#include "MTTable.h"
using namespace Rcpp;

MTTable::MTTable()
  : x(), y(), filepath(), source_filepath("") {}

MTTable::MTTable(std::vector<int> x, std::vector<int> y, std::vector<std::string> filepath, std::string source_filepath){
  this->x = x;
  this->y = y;
  this->filepath = filepath;
  this->source_filepath = source_filepath;
}

void MTTable::r_initiate(Rcpp::String r_filepath) {

  MTTable::set_source_filepath(r_filepath);

  MTTable::CsvToMTBin();

}

void MTTable::set_source_filepath(Rcpp::String r_filepath) {
  // std::string r_filevar = static_cast<std::string>(r_filepath);
  // this->filepath[0] = r_filevar;
  this->source_filepath = static_cast<std::string>(r_filepath);
}

std::string MTTable::get_source_filepath() {
  return source_filepath;
}

// not defined in the class, but helper for csv line reading
void process_csv_line(MTSubTable& subtable, const char* start, const char* end) {
  std::string line(start, end);

  // vector to store the individual values
  std::vector<MTTable::DataType> values;

  // use stringstream to parse the line
  std::stringstream ss(line);
  std::string item;

  while (std::getline(ss, item, ',')) {
    values.push_back(item);
  }

  // add newly read row into data frame
  const std::vector<MTTable::DataType>& vals = values;
  subtable.table_add_row(vals);

}

int MTTable::CsvToMTBin() {

  MTSubTable subtable(4, 3, 0.0, 0, 0);

  int fd = open(get_source_filepath().c_str(), O_RDONLY);
  if (fd == -1) {
    perror("open");
    return 1;
  }

  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    perror("fstat");
    close(fd);
    return 1;
  }

  size_t length = sb.st_size;
  void* addr = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
  if (addr == MAP_FAILED) {
    perror("mmap");
    close(fd);
    return 1;
  }

  char* data = static_cast<char*>(addr);
  const char* start = data;
  const char* end = data + length;

  while (start < end) {
    const char* newline = static_cast<const char*>(memchr(start, '\n', end - start));
    if (newline) {
      process_csv_line(subtable, start, newline);
      start = newline + 1;
    } else {
      process_csv_line(subtable, start, end);
      break;
    }
  }

  //test line, remove
  subtable.get_df().print();

  if (munmap(addr, length) == -1) {
    perror("munmap");
  }

  // write a subtable to binary file
  if (std::filesystem::create_directory("mt")) {
    Rcout << "New mt/ directory created successfully." << std::endl;
  } else {
    Rcout << "Failed to create directory mt/." << std::endl;
    return 1;
  }

  std::ofstream ofs("mt/subtable.bin", std::ios::binary);
  if (!ofs) {
    Rcout << "Failed to open mt/subtable.bin file for writing." << std::endl;
    return 1;
  }

  ofs.write(reinterpret_cast<const char*>(&subtable), sizeof(subtable));

  ofs.close();

  MTSubTable sb2;

  // readMTBinSubTable("mt/subtable.bin", sb2);

  // sb2.get_df().print();

  close(fd);

  return 0;

}

int MTTable::readMTBinSubTable(const std::string& filename, MTSubTable& subtable) {

  std::ifstream ifs(filename, std::ios::binary);
  if (!ifs) {
    Rcout << "Failed to read MTBin into MTSubFile; could not open file." << std::endl;
    return 1;
  }

  ifs.read(reinterpret_cast<char*>(&subtable), sizeof(subtable));
  ifs.close();

  return 0;
}