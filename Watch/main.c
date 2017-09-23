#include <pebble.h>

Window *window;
TextLayer *text_layer;
TextLayer *text_layer_main;
char msg[128];
char msg_main[128];
char appMsg[30];
bool enable_flush  = true;
bool stand_by_mode = false; 
int  threshold = 30;

void out_sent_handler(DictionaryIterator *sent, void *context) { 
  // outgoing message was delivered -- do nothing
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
// outgoing message failed
  text_layer_set_text(text_layer_main, "Error!");
  text_layer_set_text(text_layer,"Error in sending message");
}

void in_received_handler(DictionaryIterator *received, void *context) {
  // incoming message received 
  
  if (!enable_flush) {
    return;
  }
  
  if (stand_by_mode) {
    char* isWarning = dict_find(received, 18) -> value -> cstring;
    if (isWarning[0] == '1') {
      text_layer_set_text(text_layer_main,"WARNING!");
      vibes_short_pulse();
    }
    return;
  }
  
  char* response  = dict_find(received, 12) -> value -> cstring;
  if (response[0] == '0') {
    text_layer_set_text(text_layer, "Disconnect from server");
    text_layer_set_text(text_layer_main, "Error!");
    return;
  }
  
  char* isConnect = dict_find(received, 17) -> value -> cstring;
  if (isConnect[0] == '1') {
    
    char* low    = dict_find(received, 13) -> value -> cstring;
    char* avg    = dict_find(received, 15) -> value -> cstring;
    char* high   = dict_find(received, 14)-> value -> cstring;
    char* recent = dict_find(received, 16)->value-> cstring;
    
    snprintf(msg     , 128, "low : %s\nhigh: %s\navg : %s\n", low,high,avg);
    snprintf(msg_main, 128,"%s",recent);
    text_layer_set_text(text_layer, msg);
    text_layer_set_text(text_layer_main, msg_main);
    
  } else {
    text_layer_set_text(text_layer,"Disconnect from Arduino");
    text_layer_set_text(text_layer_main, "Error!");
  }
}

void in_dropped_handler(AppMessageResult reason, void *context) { 
  // incoming message dropped
  text_layer_set_text(text_layer_main, "Error!");
  text_layer_set_text(text_layer,"Error in receiving message");
}

void send_appmessage(int key, char* str) {
  DictionaryIterator *iter; 
  app_message_outbox_begin(&iter);
  Tuplet value = TupletCString(key, str); 
  dict_write_tuplet(iter, &value);
  app_message_outbox_send();    
}


// void up_and_down_click_handler(ClickRecognizerRef recognizer, void *context) {
//   if (!stand_by_mode) {
//     send_appmessage(3,"1");
//   }
// }

void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!stand_by_mode) {
    send_appmessage(1,"1");
  }else {
    threshold++;
    snprintf(msg, 128, "Stand-by mode\n threshold : %d c", threshold);
    text_layer_set_text(text_layer,msg);  }
}

void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!stand_by_mode) {
    send_appmessage(1,"1");
  }else {
    threshold--;
    snprintf(msg, 128, "Stand-by mode\n threshold : %d c", threshold);
    text_layer_set_text(text_layer,msg);  }
}

void long_select_hold_handler(ClickRecognizerRef recognizer, void *context) {
  enable_flush = false;
  text_layer_set_text(text_layer_main,"");  
  
  char* mode_change;
  if (stand_by_mode) {
    text_layer_set_text(text_layer,"release to active mode");
    mode_change = "4"; //resume
  }else {
    text_layer_set_text(text_layer,"release to stand-by mode");
    mode_change = "3"; //stand by
  }
  send_appmessage(3,mode_change);  
  stand_by_mode = !stand_by_mode;
}

void long_select_release_handler(ClickRecognizerRef recognizer, void *context) {
  if (stand_by_mode) {
    snprintf(msg, 128, "Stand-by mode\n threshold : %d c", threshold);
    text_layer_set_text(text_layer,msg);
    text_layer_set_text(text_layer_main,"zzZ");
  }
  enable_flush = true;
}

void double_click_select_handler(ClickRecognizerRef recognizer, void *context) {
  enable_flush = false;
  text_layer_set_text(text_layer_main,"Quit");
  text_layer_set_text(text_layer, "server shut down");
}

void config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP       , up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN     , down_click_handler);
  window_long_click_subscribe  (BUTTON_ID_SELECT, 0, long_select_hold_handler, long_select_release_handler);
  window_multi_click_subscribe (BUTTON_ID_SELECT, 2, 0, 0, true, double_click_select_handler);
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
  if (enable_flush && !stand_by_mode) {
    send_appmessage(0, "0");
  }
  if (enable_flush && stand_by_mode) {
    char str[30];
    snprintf(str ,30, "2/%dc", threshold);
    send_appmessage(2,str); 
  }
  
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  text_layer  = text_layer_create(
    (GRect) { 
       .origin = { 0, 0 },
       .size   = { bounds.size.w, bounds.size.h - 60} 
    }); 
  
  text_layer_main = text_layer_create(
    (GRect) { 
       .origin = { 0,  bounds.size.h - 60},
       .size   = { bounds.size.w, 60 } 
    });
  text_layer_set_text_alignment(text_layer     , GTextAlignmentLeft); 
  text_layer_set_text_alignment(text_layer_main, GTextAlignmentLeft); 
  
  text_layer_set_font(text_layer_main, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));

  layer_add_child(window_layer, text_layer_get_layer(text_layer));
  layer_add_child(window_layer, text_layer_get_layer(text_layer_main)); 
}

static void window_unload(Window *window) { 
  text_layer_destroy(text_layer);
  text_layer_destroy(text_layer_main);
}


void handle_init(void) {
  window = window_create(); 
  window_set_window_handlers(window, 
    (WindowHandlers) {
      .load = window_load,
      .unload = window_unload
    });
  
  window_set_click_config_provider(window, config_provider);
  
  // for registering AppMessage handlers 
  app_message_register_inbox_received(in_received_handler); 
  app_message_register_inbox_dropped (in_dropped_handler); 
  app_message_register_outbox_sent   (out_sent_handler); 
  app_message_register_outbox_failed (out_failed_handler); 
  const uint32_t inbound_size = 64;
  const uint32_t outbound_size = 64; 
  app_message_open(inbound_size, outbound_size);
  
  tick_timer_service_subscribe(SECOND_UNIT, (TickHandler)tick_handler);
  
  window_stack_push(window, true); 
  
}

void handle_deinit(void) {
  window_destroy(window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
