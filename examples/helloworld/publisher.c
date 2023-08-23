#include "dds/dds.h"
#include "HelloWorldData.h"
#include <stdio.h>
#include <stdlib.h>
#include "dds/ddsrt/threads.h"

static unsigned int threadfunc (void *arg)
{
  (void) arg;
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t writer;
  dds_return_t rc;
  HelloWorldData_Msg msg;
  uint32_t status = 0;

  /* Create a Participant. */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0)
    return 1;

  /* Create a Topic. */
  topic = dds_create_topic (
    participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL);
  if (topic < 0)
    return 1;

  /* Create a Writer. */
  writer = dds_create_writer (participant, topic, NULL, NULL);
  if (writer < 0)
    return 2;

  printf("=== [Publisher]  Waiting for a reader to be discovered ...\n");
  fflush (stdout);

  rc = dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS);
  if (rc != DDS_RETCODE_OK)
    return 3;

  while(!(status & DDS_PUBLICATION_MATCHED_STATUS))
  {
    rc = dds_get_status_changes (writer, &status);
    if (rc != DDS_RETCODE_OK)
      return 4;

    /* Polling sleep. */
    dds_sleepfor (DDS_MSECS (20));
  }

  /* Create a message to write. */
  msg.userID = 1;
  msg.message = "Hello World";

  printf ("=== [Publisher]  Writing : ");
  printf ("Message (%"PRId32", %s)\n", msg.userID, msg.message);
  fflush (stdout);

  rc = dds_write (writer, &msg);
  if (rc != DDS_RETCODE_OK)
    return 5;

  /* Deleting the participant will delete all its children recursively as well. */
  rc = dds_delete (participant);

  return 0;
}

int main (int argc, char ** argv)
{
  (void)argc;
  (void)argv;

  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init (&tattr);

  ddsrt_thread_t pub_tid;

  dds_return_t rc = ddsrt_thread_create (&pub_tid, "pub_thread", &tattr, threadfunc, NULL);
  if (rc != DDS_RETCODE_OK)
    return EXIT_FAILURE;

  return DDS_RETCODE_OK != ddsrt_thread_join (pub_tid, NULL);
}
