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

(1) Write flow (loaned):
- acquire a loaned block from a writer
  - the virtual interface will keep track of this block
  - the writer becomes the owner of the block
  - the refcount of the block is now 1

- populate the loaned block with values treating it as a sample to be written

- call the write on the sample/block (3)

(2) Write flow (unloaned):
- create a sample in the main code

- populate the sample with values

- call the write on the sample/block (3)

(3) Internal write logic (dds_write_impl):
- the sample pointer handed over to write is checked whether it is a block loaned by one of the pipes
  - if it is, create a serdata from a loaned sample (4)
  - if it is not, attempt to get a loan for a number of bytes:
    - for fixed size types, just the fixed size
    - for others, the (CDR) serialized size
- if a loan is detected, create a serdata from a loaned sample (4)
- if no loan is detected, create a serdata from a normal sample (5)
- populate the values of the serdata further
  - statusinfo
  - timestamp
- call the network write (6)
- attempt to sink the data through a pipe (if any) (7)
  - the pipe used is:
    - the pipe the block was loaned from if a loan is present
    - or the first pipe in the list if any are present

(4) creating a serdata from a loaned sample (ddsi_serdata_from_loaned_sample):
- if the data needs to be serialized (network readers):
  - create a serdata as usual
  - else create an empty serdata and just populate the key value
- set the loan pointer on the serdata and check if the pointer to the loaned block and the sample are the sample
  - if they are, the loaned block is already populated, and nothing needs to be done
  - if they are not:
    - if the data was serialized:
        - copy the data from the serdata to the loaned block
        - otherwise copy the data from the sample to the loaned block

(5) creating a serdata from a normal sample (ddsi_serdata_from_sample):
- create a serdata as usual

(6) network write (dds_write_basic_impl)
- only write to the network if there are remote readers

(7) sink data through a pipe (sink_data)
- if the serdata has a loan:
  - the ownership of the loaned block transfers back to the virtual interface
  - at this point the refcount of the block is still 1
- if there is no loan:
  - implementation specific, might not even be supported for the virtual interface


Typical read flow:
(1) the block is moved from the virtual interface to the reader:
- by callback function (event based implementation) (2)
- by actively pulling from the interface (polling implementation) (3)
- at this point the ownership transfers from the virtual interface to the reader
  - multiple readers can share ownership of the same block
  - only if all readers have returned the block to the virtual interface can it be recycled

(2) insertion of external data in reader history cache

(3) pulling data from a virtual interface

(4) at the point of reading
- samples provided for read output are checked for their origin


Write scenarios and performance:
- fixed size types:
  - writing a loaned sample:
    - w/network writes:
      - serialized sample generation: O(sample.all_data)
    - wo/network writes:
      - key generation from sample: O(sample.key_data)
  - writing a non-loaned sample:
    - w/network writes:
      - serialized sample generation: O(sample.all_data)
      - copy sample to loaned block: O(sample.all_data)
    - wo/network writes:
      - key generation from sample: O(sample.key_data)
      - copy sample to loaned block: O(sample.all_data)
- dynamic size types:
  - writing a non-loaned sample:
      - w/network writes:
        - serialized sample generation: O(sample.all_data)
        - copy serialized sample to loaned block: O(sample.all_data)
      - wo/network writes:
        - serialized sample generation: O(sample.all_data)
        - copy serialized sample to loaned block: O(sample.all_data)


Read scenarios and performance:
- fixed size types:
  - reading into a loaned sample:
     - just exchange pointers O(0)
  - reading into a non-loaned sample:
     - copy into sample O(sample.all_data)
- dynamic size types:
  - reading into a loaned sample:
     - deserialization of buffer O(sample.all_data)
  - reading into a non-loaned sample:
     - deserialization of buffer O(sample.all_data)
