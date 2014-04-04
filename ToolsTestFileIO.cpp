/* 
 * File:   ToolsTestFileIO.cpp
 * Author: kjell
 *
 * Created on August 15, 2013, 3:41 PM
 */

#include "ToolsTestFileIO.h"
#include "FileIO.h"
#include <random>
#include <cstdio>
#include <functional>
#include "StopWatch.h"
#include <algorithm>

namespace {
   // Random integer function from http://www2.research.att.com/~bs/C++0xFAQ.html#std-random

   int random_int(int low, int high) {
      using namespace std;
      static std::random_device rd; // Seed with a real random value, if available
      static default_random_engine engine{rd()};
      typedef uniform_int_distribution<int> Distribution;
      static Distribution distribution{};
      return distribution(engine, Distribution::param_type{low, high});
   }
   struct ScopedFileCleanup {
      const std::string file;
      explicit ScopedFileCleanup(const std::string& name) : file(name) { }
      ~ScopedFileCleanup() {
         remove(file.c_str());
      }
   };
   
   
}

TEST_F(TestFileIO, CannotOpenFileToRead) {
   auto fileRead = FileIO::ReadAsciiFileContent({"/xyz/*&%/x.y.z"});
   EXPECT_TRUE(fileRead.result.empty());
   EXPECT_FALSE(fileRead.error.empty());
   EXPECT_TRUE(fileRead.HasFailed());
}

TEST_F(TestFileIO, CanOpenFileToRead) {
   auto fileRead = FileIO::ReadAsciiFileContent({"/proc/stat"}); // fine as long as we are on *nix systems
   EXPECT_FALSE(fileRead.result.empty());
   EXPECT_TRUE(fileRead.error.empty());
   EXPECT_FALSE(fileRead.HasFailed());
}

TEST_F(TestFileIO, CannotWriteToFile) {
   auto fileWrite = FileIO::WriteAsciiFileContent({"xyz/123/proc/stat"},
   {
      "Hello World"
   });
   EXPECT_FALSE(fileWrite.result);
   EXPECT_FALSE(fileWrite.error.empty());
   EXPECT_TRUE(fileWrite.HasFailed());
}

TEST_F(TestFileIO, CanWriteToFileAndReadTheFile) {
   using namespace std;

   string filename{"/tmp/TestFileIO_"};
   filename.append(to_string(random_int(0, 1000000)))
           .append({"_"})
   .append(to_string(random_int(0, 1000000)));

   // cleanup/removing the created file when exiting
   ScopedFileCleanup cleanup{filename};

   auto fileWrite = FileIO::WriteAsciiFileContent(filename,{"Hello World"});
   EXPECT_TRUE(fileWrite.result);
   EXPECT_TRUE(fileWrite.error.empty());
   EXPECT_FALSE(fileWrite.HasFailed());

   auto fileRead = FileIO::ReadAsciiFileContent(filename);
   EXPECT_EQ("Hello World", fileRead.result);
   EXPECT_TRUE(fileRead.error.empty());
   EXPECT_FALSE(fileRead.HasFailed());
}

TEST_F(TestFileIO, FileIsNotADirectory) {
   std::string filename{"/tmp/123_456_789"};
   {
      ScopedFileCleanup cleanup{filename};
      EXPECT_FALSE(FileIO::DoesFileExist(filename));
      EXPECT_FALSE(FileIO::DoesDirectoryExist(filename));

      auto fileWrite = FileIO::WriteAsciiFileContent(filename,{"Hello World"});
      EXPECT_TRUE(fileWrite.result);
      EXPECT_TRUE(fileWrite.error.empty());
      EXPECT_FALSE(fileWrite.HasFailed());

      EXPECT_TRUE(FileIO::DoesFileExist(filename));
      EXPECT_FALSE(FileIO::DoesDirectoryExist(filename));
   }
   // RAII cleanup
   EXPECT_FALSE(FileIO::DoesFileExist(filename));

}

TEST_F(TestFileIO, DirectoryExistance) {
   std::string directory{"/tmp/some_temp_directory"};
   {
      EXPECT_FALSE(FileIO::DoesDirectoryExist(directory)) << directory;
      std::string createDir{"mkdir -p " + directory};
      EXPECT_EQ(0, system(createDir.c_str()));

      ScopedFileCleanup cleanup{directory};
      EXPECT_TRUE(FileIO::DoesFileExist(directory));
      EXPECT_TRUE(FileIO::DoesDirectoryExist(directory));
   }
   // RAII cleanup
   EXPECT_FALSE(FileIO::DoesFileExist(directory));
   EXPECT_FALSE(FileIO::DoesDirectoryExist(directory));
}


TEST_F(TestFileIO, DirectoryReader_NotExistingDirectory) {
   FileIO::DirectoryReader reader{mTestDirectory + "/_#Does_not+_exist"};
   EXPECT_TRUE(reader.Valid().HasFailed()) << reader.Valid().error;
}

TEST_F(TestFileIO, DirectoryReader_ExistingDirectory) {
   FileIO::DirectoryReader reader{mTestDirectory};
   EXPECT_FALSE(reader.Valid().HasFailed()) << reader.Valid().error;
}


// An empty directory will only contain "." and ".." which we ignores
TEST_F(TestFileIO, DirectoryReader_NoFilesInDirectory) {
   FileIO::DirectoryReader reader{mTestDirectory};
   EXPECT_FALSE(reader.Valid().HasFailed());
   
   FileIO::DirectoryReader::Found fileAndType = reader.Next();
   EXPECT_EQ(fileAndType.first, FileIO::DirectoryReader::TypeFound::End);
   EXPECT_EQ(fileAndType.second, "");
}


TEST_F(TestFileIO, DirectoryReader_HasFilesInDirectory__AfterReset) {
   using namespace FileIO;  
   
   DirectoryReader reader{mTestDirectory};
   DirectoryReader::Found fileAndType = reader.Next(); 
   
   EXPECT_EQ(fileAndType.first, DirectoryReader::TypeFound::End);
   EXPECT_EQ(fileAndType.second, "");
      
   // We have already reached the end. This must be reset before reading successfully
   CreateFile(mTestDirectory, "some_file");
   fileAndType = reader.Next();
   EXPECT_EQ(fileAndType.first, DirectoryReader::TypeFound::End);
   EXPECT_EQ(fileAndType.second, "");
   
   // After the reset we can find the file
   reader.Reset();
   fileAndType = reader.Next();   
   EXPECT_EQ(fileAndType.first, DirectoryReader::TypeFound::File);
   EXPECT_EQ(fileAndType.second, "some_file");
  

   // has reached the end again
   fileAndType = reader.Next();
   EXPECT_EQ(fileAndType.second, "");
   EXPECT_EQ(fileAndType.first, DirectoryReader::TypeFound::End);
   
   
   CreateSubDirectory("some_directory");
   EXPECT_TRUE(FileIO::DoesDirectoryExist({mTestDirectory + "/some_directory"}));
   reader.Reset();
   
   fileAndType = reader.Next();
   EXPECT_NE(fileAndType.first, DirectoryReader::TypeFound::End);
   
   std::string filename;
   std::string directoryname;

   for (size_t count = 0; count < 2; ++count) {
      if (fileAndType.first == DirectoryReader::TypeFound::Directory) {
         directoryname = fileAndType.second;
         fileAndType = reader.Next();
      }

      if (fileAndType.first == DirectoryReader::TypeFound::File) {
         filename = fileAndType.second;
         reader.Next();
      }
   }   
   
   EXPECT_EQ(filename, "some_file");
   EXPECT_EQ(directoryname, "some_directory");
   fileAndType = reader.Next();
   
   EXPECT_EQ(fileAndType.first, DirectoryReader::TypeFound::End);
   EXPECT_EQ(fileAndType.second, "");
}

   //   for (size_t index = 0; index < 1000; ++index) {
   //  CreateSubDirectory(std::to_string(index));      
  // }
TEST_F(TestFileIO, AThousandFiles) {
   using namespace FileIO;  
     
   for (size_t index = 0; index < 1000; ++index) {
      CreateFile(mTestDirectory, std::to_string(index));
   }
   
   
   std::vector<std::string> files;
   DirectoryReader::Found entry;
   
   DirectoryReader reader(mTestDirectory);  
   StopWatch timeToFind;
   entry = reader.Next();
   while(entry.first != DirectoryReader::TypeFound::End) {
      ASSERT_NE(entry.first, DirectoryReader::TypeFound::Directory);
      ASSERT_NE(entry.first, DirectoryReader::TypeFound::Unknown);
      files.push_back(entry.second);
      entry = reader.Next();
   }
   
   ASSERT_EQ(files.size(), 1000);
   std::cout << "Time to find 1000 files and save them took: " << timeToFind.ElapsedSec() << std::endl;

   std::sort(files.begin(), files.end(), [](const std::string& lh, const std::string& rh){
      return std::stoul(lh) < std::stoul(rh);
   });
   for(size_t index = 0; index < 1000; ++index) {
      EXPECT_EQ(files[index], std::to_string(index));
   }
}
   