Virtual interface concept

Terms:
backend: the library implementing the virtual interface
CDDS: CycloneDDS
virtual interface: the entry point of a CDDS domain in the backend
virtual interface topic: the entry point of a CDDS topic in the virtual interface
virtual interface pipe: the entry point of a CDDS reader/writer in the virtual interface topic

Configure the virtual interface through the config file
- per domain, a number of virtual interfaces can be loaded from the supplied libraries

Virtual interface expected behaviour:
- the virtual interface will only exchange data with other instances which have the same CDDS domain id
- on each virtual interface topics can be created for each CDDS topic created on the domain if the topic's QoS
  settings allow the virtual interface
  
Virtual interface topic behaviour
- the virtual interface topic is the counterpart of the CDDS topic
- the virtual interface topic will only exchange data with the same topic names and sertype_hashes
- for readers/writers on the topic pipes can be created
- the topic can be queried whether serialization is necessary, this occurs when the serdata basehashes
  of the different topics with the same name have different serdata basehashes
advise:
- keep track of the existing virtual interfaces in the backend

Virtual interface pipe behaviour
- the pipe is the access point for CDDS to access data through the virtual interface
- the pipe allows the writer to loan blocks of data if the virtual interface supports loan semantics
- a callback function can be attached to a pipe which is triggered when data is received on it
//- the library implementation side requires an administration per pipe for loaned blocks, if loans are supported

Memory block behaviour
- the memory block is a block of memory originating from a pipe
- the pipe can be queried if a pointer to a sample belongs to a block supplied from the pipe
- the block contains information of the hash of the sertype information and whether it is serialized
- if it is serialized, it can be deserialized using the CDR deserialization methods
