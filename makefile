google_test=googletest-release-1.8.1

gtest_include=$(google_test)/googletest/include
gtest_lib_dir=$(google_test)/googletest/lib/.libs
gtest_lib=libgtest.a

gmock_include=$(google_test)/googlemock/include
gmock_lib_dir=$(google_test)/googlemock/lib/.libs
gmock_lib=libgmock.a

gtest_main_lib=libgtest_main.a

protobuf=./protobuf-3.5.1

protoc=$(protobuf)/src/.libs/protoc

protobuf_lib_dir=$(protobuf)/src/.libs
protobuf_lib=$(protobuf_lib_dir)/libprotobuf.a

#LDLIBS+= -L$(gtest_lib_dir) $(gtest_lib)
LDLIBS+= $(protobuf_lib)
#LDLIBS+= $(gtest_main_lib)
#LDLIBS+= -L$(gmock_lib_dir) $(gmock_lib) -lpthread
LDLIBS+= -L/opt/bb/lib64/ -lboost_system -lboost_program_options -lpthread

CXXFLAGS+= -g -I$(gtest_include) -I$(gmock_include) -std=c++14
CXXFLAGS+= -I/opt/bb/include
CXXFLAGS+= -D_GLIBCXX_USE_CXX11_ABI=0

%.pb.cc: %.proto
	$(protoc) --cpp_out=. $< 

OBJS=proto.pb.cc proto-part.o
OBJS=print-server.o proto.pb.cc

test: $(OBJS)
