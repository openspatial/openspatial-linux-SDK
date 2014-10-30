/*
 * Copyright 2014, Nod Labs
 *
 * This is main file of the test framework, which can be used as a farm test
 * of the devices
 */
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <bits/socket.h>
#include <glib.h>

#include <bluez/bluetooth/bluetooth.h>
#include <bluez/bluetooth/uuid.h>
#include <bluez/bluetooth/btio.h>
#include <bluez/bluetooth/hci.h>
#include <bluez/bluetooth/hci_lib.h>
#include <bluez/gatt/gattrib.h>
#include <bluez/gatt/att.h>
#include <bluez/gatt/gatt.h>
#include <bluez/gatt/utils.h>

#define ERR_CONNECT_FAILED  0xff
#define ERR_DISCONNECTED    0xfe
#define ERR_IN_PROGRESS     0xfd
#define ERR_SEC_LEVEL       0xfc
#define ERR_PROTOCOL        0xfb
#define ERR_MTU             0xfa
#define SEC_LEVEL           "high"    /* we need this level for enabling the HID notifications. But this can lead to
                                       * closure of the iochannel handler
                                       */
#define CHAR_START          0x0001
#define CHAR_END            0x00FF
#define ENABLE_NOTIFICATION   "01 00"
#define DISABLE_NOTIFICATION  "00 00"

struct items {
  guint8 error_num;
  char error_str[127];
  long n_items;
  guint8 data[0];
};

static enum state {
  STATE_SCANNING,
  STATE_CONNECTING,
  STATE_CONNECTED,
  STATE_CONNACTIVE,
  STATE_DATARCVD,
  STATE_DISCONNECTED,
} conn_state;

enum write_type {
  WRITE_COMMAND,    /* write without response */
  WRITE_REQUEST,    /* write with response */
};

static uint8_t filter_dup = 1;
static int finish_scanning, app_quit;
static struct hci_filter old_filter;
static uint16_t mtu;

uint16_t notified_handle = 0;
uint16_t notified_len = 0;
uint8_t notified_data[128];
uint16_t indicated_handle = 0;
uint16_t indicated_len = 0;
uint8_t indicated_data[128];

static GIOChannel *iochannel = NULL;
static GMainLoop *event_loop;
GAttrib *attrib;

static void set_state(enum state st)
{
  conn_state = st;
}

static enum state get_state(void)
{
  return conn_state;
}

static void signal_handler(int sig)
{
  printf("\nHandling the interrupt...\n");
  /* some error caused user to hit ^C */
  if (get_state() == STATE_SCANNING)
    finish_scanning = 1;
  else if(get_state() == STATE_DATARCVD)
    g_main_loop_quit(event_loop);
  else
    exit(-1);

  return;
}

static void set_error(guint8 num, const char *msg, gpointer user_data)
{
    struct items *p = (struct items *)user_data;
    if (p) {
      p->error_num = num;
      if (msg)
        strncpy(p->error_str, msg, 127);
      else
        *p->error_str = '\0';
    } else
      printf("%s\n", msg);

    return;
}

int address2string(const bdaddr_t *ba, char *str)
{
  return sprintf(str, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
    ba->b[5], ba->b[4], ba->b[3], ba->b[2], ba->b[1], ba->b[0]);
}

static void events_handler(const uint8_t *pdu, uint16_t len, gpointer user_data)
{
    GString *s;

    GAttrib *attrib = user_data;
    uint8_t *opdu;
    uint16_t handle, i, olen = 0;
    size_t plen;

    handle = att_get_u16(&pdu[1]);

    switch (pdu[0]) {
      case ATT_OP_HANDLE_NOTIFY:
        printf("Notification handle = 0x%04x value: ", handle);
        s = g_string_new(NULL);
        g_string_printf(s, "Notification handle = 0x%04x value: ", handle);
        break;

      case ATT_OP_HANDLE_IND:
        printf("Indication   handle = 0x%04x value: ", handle);
        s = g_string_new(NULL);
        g_string_printf(s, "Indication   handle = 0x%04x value: ", handle);
        break;

      default:
        printf("Invalid opcode\n");
        return;
    }

    for (i = 3; i < len; i++)
      printf("%02x ", pdu[i]);
    printf("\n");

    set_state(STATE_DATARCVD);

    return;
}

void disconnect_io()
{
    printf("disconnect_io()\n");

    if (get_state() == STATE_DISCONNECTED)
      return;

    g_attrib_unref(attrib);
    attrib = NULL;
    mtu = 0;

    g_io_channel_shutdown(iochannel, FALSE, NULL);
    g_io_channel_unref(iochannel);
    iochannel = NULL;

    set_state(STATE_DISCONNECTED);
}

static gboolean channel_hangup_watcher(GIOChannel *chan, GIOCondition cond,
                                gpointer user_data)
{
    set_error(ERR_DISCONNECTED, "Disconnected\n", user_data);
    disconnect_io();
    return 0;
}

void btcmd_stop_scanning(int hci_dev)
{
  setsockopt(hci_dev, SOL_HCI, HCI_FILTER, &old_filter, sizeof(old_filter));

  hci_le_set_scan_enable(hci_dev, 0x00, filter_dup, 2000);
  hci_close_dev(hci_dev);

  return;
}

static int cmd_lescan(int dev_id)
{
   int hci_dev = 0;
   int err = 0, opt = 0, dd = 0;
   uint8_t own_type = 0x00;
   uint8_t scan_type = 0x01;
   uint8_t filter_type = 0;
   uint8_t filter_policy = 0x00;
   uint16_t interval = htobs(0x0010);
   uint16_t window = htobs(0x0010);
   uint8_t filter_dup = 1;

   if (dev_id < 0)
       dev_id = hci_get_route(NULL);

   if (dev_id < 0)
       return -1;

   if((hci_dev = hci_open_dev(dev_id)) < 0) {
       printf("Opening hci device failed\n");
       return -2;
   }

   err = hci_le_set_scan_parameters(hci_dev, scan_type, interval, window, own_type, filter_policy, 2000);
   if (err < 0) {
      printf("Setting scan parameters failed %d\n", err);
      return -3;
   }

   err = hci_le_set_scan_enable(hci_dev, 0x01, filter_dup, 1000);
   if (err < 0) {
      printf("Enabling the scan failed\n");
      return -4;
   }

   struct sigaction sa;
   sa.sa_flags = SA_NOCLDSTOP;
   sa.sa_handler = signal_handler;
   sigaction(SIGINT, &sa, NULL);
   unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
   int len, sl_no = 0;

   socklen_t olen = sizeof(old_filter);

   if (getsockopt(hci_dev, SOL_HCI, HCI_FILTER, &old_filter, &olen) < 0) {
       printf("getsockopt failed\n");
       btcmd_stop_scanning(hci_dev);
       return -5;
   }

   struct hci_filter filter;
   hci_filter_clear(&filter);
   hci_filter_set_ptype(HCI_EVENT_PKT, &filter);
   hci_filter_set_event(EVT_LE_META_EVENT, &filter);

   if (setsockopt(hci_dev, SOL_HCI, HCI_FILTER, &filter, sizeof(filter)) < 0) {
      printf("setsockopt failed\n");
      btcmd_stop_scanning(hci_dev);
      return -6;
   }

   finish_scanning = 0;

   while(1) {
    le_advertising_info *info;
    evt_le_meta_event *meta;
    char addr[20];

    while ((len = read(hci_dev, buf, sizeof(buf))) < 0) {
      set_state(STATE_SCANNING);
      if (errno == EINTR && finish_scanning) {
        len = 0;
        goto done;
       }
       if (errno == EAGAIN)
          continue;
       /* anything else, just end the loop */
       goto done;
    } 

     ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
     len -= (1 + HCI_EVENT_HDR_SIZE);

     meta = (void *) ptr;

     if (meta->subevent != 0x02)
      goto done;

     info = (le_advertising_info *) (meta->data + 1);
    
     memset(addr, 0, sizeof(addr));

     address2string(&info->bdaddr, addr);
     /* Socket gives same BT address twice, and we need to eliminate
      * duplicate entries
      */
     static int duplicate = 0;
     if (duplicate == 0) {
       printf("%s\n", addr);
       duplicate = 1;
     } else if (duplicate)
       duplicate = 0;
    } /* while */
done:
    btcmd_stop_scanning(hci_dev);

    set_state(STATE_DISCONNECTED);

    return 0;
}

static void discover_char_cb(GSList *characteristics, guint8 status, gpointer user_data)
{
  GSList *l;

  if (status) {
    printf("Discover all characteristics failed: %s\n", att_ecode2str(status));
    goto done1;
  }

  for (l = characteristics; l; l = l->next) {
    struct gatt_char *chars = l->data;
    printf("handle = 0x%04x, properties = 0x%02x, value handle = 0x%04x, uuid = %s\n", chars->handle,chars->properties, chars->value_handle, chars->uuid);
  }
done1:
    g_main_loop_quit(event_loop);
}

static void discover_services_cb(GSList *services, guint8 status, gpointer user_data)
{
    GSList *l;

    if (status) {
      printf("Discover primary services by UUID failed: %s\n", att_ecode2str(status));
      goto done;
    }

    if (services == NULL) {
      printf("No service UUID found\n");
      goto done;
    }

    for (l = services; l; l = l->next) {
      struct gatt_primary *prim = l->data;
      printf("start handle: 0x%04x end handle: 0x%04x uuid: %s\n", prim->range.start, prim->range.end, prim->uuid);
    }    
    
done:
    g_main_loop_quit(event_loop);
}

static int discover_characteristics(gpointer data, uint16_t start, uint16_t end)
{
    GAttrib *attrib = data;

    if (get_state() != STATE_CONNECTED) {
      printf("device is already disconnected\n");
      return -1;
    }

    gatt_discover_char(attrib, start, end, NULL, discover_char_cb, NULL);

    g_main_loop_run(event_loop);

    return 0;
}

static int discover_services(gpointer data)
{
    GAttrib *attrib = data;

    if (get_state() != STATE_CONNECTED) {
      printf("device is already disconnected\n");
      return -1;
    }    
    gatt_discover_primary(attrib, NULL, discover_services_cb, NULL);
    g_main_loop_run(event_loop);
    return 0;
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data)
{
  if (err) {
    set_error(ERR_CONNECT_FAILED, err->message, user_data);
    set_state(STATE_DISCONNECTED);

  } else {
    /* attrib is declared as global for now */
    attrib = g_attrib_new(iochannel);
    g_attrib_register(attrib, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES, events_handler, attrib, NULL);
    g_attrib_register(attrib, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES, events_handler, attrib, NULL);
    set_state(STATE_CONNECTED);
  }
  g_main_loop_quit(event_loop);
}

static int cmd_connect(const char *dst, gpointer user_data)
{
  bdaddr_t sba, dba;
  uint8_t dest_type;
  BtIOSecLevel sec;

  if (get_state() != STATE_DISCONNECTED)
    return -1;

  set_state(STATE_CONNECTING);

  str2ba(dst, &dba);
  bacpy(&sba, BDADDR_ANY);            // {0, 0, 0, 0, 0, 0}
  dest_type = BDADDR_LE_PUBLIC;       // 0x01
  sec = BT_IO_SEC_LOW;                /* Connection happens at low security. Change later */

  GError *gerr = NULL;

  /* iochannel is declared global for now
   * source: src/device.c in bluez tree for more details
   */ 
  iochannel = bt_io_connect(connect_cb, user_data, NULL, &gerr,
              BT_IO_OPT_SOURCE_BDADDR, &sba,
              BT_IO_OPT_SOURCE_TYPE, BDADDR_LE_PUBLIC,
              BT_IO_OPT_DEST_BDADDR, &dba,
              BT_IO_OPT_DEST_TYPE, dest_type,
              BT_IO_OPT_CID, ATT_CID,
              BT_IO_OPT_SEC_LEVEL, sec,
              BT_IO_OPT_INVALID);

  if (gerr) {
    set_state(STATE_DISCONNECTED);
    set_error(ERR_CONNECT_FAILED, gerr->message, user_data);
    printf("Error: %d  %s\n", gerr->code, gerr->message);
    g_error_free(gerr);
    return -2;
  } else {
    g_io_add_watch(iochannel, G_IO_HUP, channel_hangup_watcher, user_data);
    g_main_loop_run(event_loop);
  }

  return 0;
}

static void cmd_disconnect()
{
    disconnect_io();
}

static int strtohandle(const char *src)
{
  char *e;
  int dst;

  errno = 0;
  dst = (int)strtoll(src, &e, 16);
  if (errno != 0 || *e != '\0')
    return -EINVAL;

  return dst;
}

static int cmd_char_write(gpointer user_data, const char *handle, const char* data, enum write_type type)
{
    GAttrib *attrib = user_data;
    uint8_t *value;
    size_t len;
    int hdl, ret;

    printf("handle = %s, data = %s, type = %d\n", handle, data, type);

    if (get_state() != STATE_CONNECTED) {
      printf("Device is not connected\n");
      return -1;
    }

    hdl = strtohandle(handle);
    if (hdl <= 0) {
      printf("A valid handle is needed\n");
      return -2;
    }

    len = gatt_attr_data_from_string(data, &value);
    if (len == 0) {
      printf("Invalid value(s) passed\n");
      return -3;
    }

    printf("handle = %d, len = %d\n", hdl, (int)len);

    if (type == WRITE_REQUEST)
      ret = gatt_write_char(attrib, hdl, value, len, NULL, NULL);
    else if (type == WRITE_COMMAND)
      ret = gatt_write_cmd(attrib, hdl, value, len, NULL, NULL);

    printf("return value = %d\n", ret);

    g_free(value);

    return 0;
}

static void char_read_cb(guint8 status, const guint8 *pdu, guint16 plen, gpointer user_data)
{
    uint8_t value[plen];
    ssize_t vlen;
    int i;
    GString *s;

    if (status != 0) {
        printf("Characteristic value/descriptor read failed: %s\n", att_ecode2str(status));
        return;
    }

    vlen = dec_read_resp(pdu, plen, value, sizeof(value));
    if (vlen < 0) {
      printf("Protocol error\n");
      return;
    }

    s = g_string_new("Characteristic value/descriptor: ");
    for (i = 0; i < vlen; i++)
      g_string_append_printf(s, "%02x ", value[i]);

    printf("%s\n", s->str);
    g_string_free(s, TRUE);

    g_main_loop_quit(event_loop);
    return;
}

static int cmd_read_char(gpointer user_data, const char *handle)
{
    GAttrib *attrib = user_data;
    int hdl, ret;

    if (get_state() != STATE_CONNECTED) {
      printf("Device is not connected\n");
      return -1;
    }

    hdl = strtohandle(handle);
    if (hdl <= 0) {
      printf("A valid handle is needed\n");
      return -2;
    }

    gatt_read_char(attrib, hdl, char_read_cb, attrib);

    g_main_loop_run(event_loop);

    return 0;
}

static int cmd_change_notify(gpointer user_data, const char *handle, const char *value)
{
  GAttrib *attrib = user_data;
  int ret = 0;

  printf("Now enabling the CCC on the handle %s\n", handle);
  if (ret = cmd_char_write(attrib, handle, value, WRITE_REQUEST) < 0)
    printf("Setting the notification enable failed %d\n", ret);
  return ret;
}

static int cmd_pair()
{

}

static int cmd_remove()
{

}

static int cmd_set_sec_level(char *level)
{
  BtIOSecLevel sec_level;
  GError *gerr = NULL;

  if (strcasecmp(level, "low") == 0)
    sec_level = BT_IO_SEC_LOW;
  else if (strcasecmp(level, "medium") == 0)
    sec_level = BT_IO_SEC_MEDIUM;
  else if (strcasecmp(level, "high") == 0)
    sec_level = BT_IO_SEC_HIGH;
  else {
      printf("Invalid security level\n");
      return -1;
  }

  if (get_state() != STATE_CONNECTED) {
    printf("Device not connected\n");
    return -2;
  }

  bt_io_set(iochannel, &gerr, BT_IO_OPT_SEC_LEVEL, sec_level, BT_IO_OPT_INVALID);

  if (gerr) {
    printf("Error: %s\n", gerr->message);
    g_error_free(gerr);
    return -3;
  }
  return 0;
}
/*
 * In the project fw, please refer to auto-generated file
 * build/erv3/fw/bcm20732/nod_db_defines.h for information on the
 * CCC handles below
 */
static const char *hdl_tbl[] = {
  "0x0004",     /* service changed */
  "0x000d",     /* battery level */
  "0x00d3",     /* HID report 1 - keyboard */
  "0x00d7",     /* HID report 2 - pointer */
  "0x0020",     /* scan refresh */
  "0x0024",     /* OTA control */
  "0x002e",     /* pose6D */
  "0x0031",     /* position 2d */
  "0x0034",     /* button state */
  "0x0037",     /* gestures */
  "0x003e",     /* nControl */
};

static const int hid_indexes[] = {2, 3};
static const int os_indexes[] = {6, 7, 8, 9, 10};
static const int ota_indexes[] = {5};
static const int bt_indexes[] = {0, 1, 4};

static inline int get_size(const int notify_array[])
{
  return (sizeof(notify_array)/sizeof(notify_array[0]));
}

static void control_service(const int notify_array[], const int size, const char *type)
{
  int i, ret;
  for (i = 0; i <= size; i++) {
    if (ret = cmd_change_notify((gpointer)attrib, hdl_tbl[notify_array[i]], type) < 0) {
      printf("Failed to set notification for handle %s\n", hdl_tbl[notify_array[i]]);
      break;
    }
    sleep(1);
    /* TODO: If we do not read back the notification handles, it somehow doesnt
     * take effect on the ring
     */
    if (ret = cmd_read_char((gpointer)attrib, hdl_tbl[notify_array[i]]) < 0)
      break;
  }
  if (ret) {
    printf("Failed in reading/writing notification value. Quitting\n");
    exit(-4);
  }
  return;
}

/* Some notifications(esp open spatial can remain enabled from the
 * previous run. Its better to disable all of them before starting
 * the current test
 */
static void disable_notifys()
{
  control_service(hid_indexes, get_size(hid_indexes), DISABLE_NOTIFICATION);
  control_service(os_indexes, get_size(os_indexes), DISABLE_NOTIFICATION);
  //control_service(ota_indexes, get_size(ota_indexes), DISABLE_NOTIFICATION);
  control_service(bt_indexes, get_size(bt_indexes), DISABLE_NOTIFICATION);
  return;
}

int main(int argc, char **argv)
{
  char addr[18];
  char *handle, *value;
  int ret, choice;
  printf("*********************************\n");
  printf("**** Nod Labs test framework ****\n");
  printf("*********************************\n");

  /* initialize a glib event loop */
  event_loop = g_main_loop_new(NULL, FALSE);

  printf("Scanning all BTLE devices in proximity...");
  printf("Press ^C to stop scanning\n");
  if(ret = cmd_lescan(0) < 0) {
    printf("Scanning failed!! Quitting %d\n", ret);
    exit(-1);
  }

  printf("Enter the device to connect: ");
  scanf("%s", addr);

  printf("Attempting to connect to [%s]\n", addr);
  ret = 0;
  if (ret = cmd_connect(addr, NULL) < 0) {
    printf("Connection failed %d\n", ret);
    exit(-2);
  }

  /* IMP: If the native bluez stack is running while this test runs, then the
   * keys get cached in the system(on which this is running). Next time this
   * test case is being run, it will fail to change the security level to
   * anything other than "low". To resolve this issue, we need to check for
   * the paired devices in bluetoothctl (command is show devices) and remove
   * the device for which this test case is being run
   */
  printf("setting the security level to %s\n", SEC_LEVEL);
  printf("The device [%s] is now connected\n", addr);
  ret = 0;
  if (ret = cmd_set_sec_level(SEC_LEVEL) < 0) {
    printf("Setting security level failed %d\n", ret);
    exit(-3);
  }
  printf("Now discovering all available services\n");
  discover_services((gpointer)attrib);

  printf("Now discovering all available characteristics\n");
  discover_characteristics((gpointer)attrib, CHAR_START, CHAR_END);

  disable_notifys();

  ret = 0;
  printf("Which service notifications you want to enable?\n");
  printf("1. HID over GATT\n");
  printf("2. Open spatial\n");
  //printf("3. DFU OTA\n");
  printf("4. Other standard services\n");
  printf("Enter your choice: ");
  scanf("%d", &choice);
  printf("\n");
  switch(choice) {
    case 1: control_service(hid_indexes, get_size(hid_indexes), ENABLE_NOTIFICATION);
            break;
    case 2: control_service(os_indexes, get_size(os_indexes), ENABLE_NOTIFICATION);
            break;
    //case 3: control_service(ota_indexes, get_size(ota_indexes), ENABLE_NOTIFICATION);
    //        break;
    case 4: control_service(bt_indexes, get_size(bt_indexes), ENABLE_NOTIFICATION);
            break;
    default: printf("Invalid choice. Enbling HID as default\n");
            control_service(hid_indexes, get_size(hid_indexes), ENABLE_NOTIFICATION);
            break;
  }
  /* We have an active connection with data ready to be received now */
  set_state(STATE_CONNACTIVE);

  /* below loop will control the event loop for receiving the data */
  g_main_loop_run(event_loop);

  printf("Disconnecting from the device\n");
  cmd_disconnect();

  /* un-initialize glib event loop */
  g_main_loop_unref(event_loop);  

  return 0;
}
