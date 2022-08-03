#include "dds/dds.h"
#include "HelloWorldData.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX_SAMPLES 8

int main (int argc, char ** argv)
{
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t writer;
  dds_return_t rc;
  uint32_t status = 0;
  (void)argc;
  (void)argv;

  /* Create a Participant. */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0)
    DDS_FATAL("dds_create_participant: %s\n", dds_strretcode(-participant));

  /* Create a Topic. */
  topic = dds_create_topic (
    participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL);
  if (topic < 0)
    DDS_FATAL("dds_create_topic: %s\n", dds_strretcode(-topic));

  /* Create a Writer. */
  writer = dds_create_writer (participant, topic, 0, NULL);
  if (writer < 0)
    DDS_FATAL("dds_create_writer: %s\n", dds_strretcode(-writer));

  printf("=== [Publisher]  Waiting for a reader to be discovered ...\n");
  fflush (stdout);

  while(!(status & DDS_PUBLICATION_MATCHED_STATUS))
  {
    rc = dds_take_status (writer, &status, DDS_PUBLICATION_MATCHED_STATUS);
    if (rc != DDS_RETCODE_OK)
      DDS_FATAL("dds_take_status: %s\n", dds_strretcode(-rc));

    /* Polling sleep. */
    dds_sleepfor (DDS_MSECS (20));
  }

  HelloWorldData_Msg *msgs[MAX_SAMPLES];

  if ((rc = dds_writer_loan_samples(writer, (void**)msgs, MAX_SAMPLES)) < 0)
    DDS_FATAL("dds_writer_loan_samples: %s\n", dds_strretcode(-rc));

  for (uint8_t sample = 0; sample < MAX_SAMPLES; sample++) {
    HelloWorldData_Msg *msg = msgs[sample];
    msg->a = sample*sample;
    msg->b = (sample*sample + 2*sample + 1);
    msg->c = (sample*sample + 4*sample + 4);
    printf ("=== [Publisher]  Writing : %p\n", msg);
    printf ("Message (a = %8d, b = %8d, c = %8d)\n", msg->a, msg->b, msg->c);
    fflush (stdout);

    rc = dds_write (writer, msg);
    if (rc != DDS_RETCODE_OK)
      DDS_FATAL("dds_write: %s\n", dds_strretcode(-rc));

    dds_sleepfor (DDS_MSECS (20));
  }

  printf ("=== [Publisher]  Waiting for reader to disappear.\n");
  while(status & DDS_PUBLICATION_MATCHED_STATUS)
  {
    rc = dds_take_status (writer, &status, DDS_PUBLICATION_MATCHED_STATUS);
    if (rc != DDS_RETCODE_OK)
      DDS_FATAL("dds_take_status: %s\n", dds_strretcode(-rc));

    //Polling sleep.
    dds_sleepfor (DDS_MSECS (20));
  }

  printf ("=== [Publisher]  No more readers, cleaning up.\n");

  /* Deleting the participant will delete all its children recursively as well. */
  rc = dds_delete (participant);
  if (rc != DDS_RETCODE_OK)
    DDS_FATAL("dds_delete: %s\n", dds_strretcode(-rc));

  return EXIT_SUCCESS;
}
