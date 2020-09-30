/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "looper.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <semaphore.h>

#define TAG "NativeCodec-looper"

struct loopermessage;
typedef struct loopermessage loopermessage;

struct loopermessage {
    int what;
    void *obj;
    loopermessage *next;
    bool quit;
};

void* looper::trampoline(void* p) {
    ((looper*)p)->loop();
    return NULL;
}

looper::looper() {
    head = NULL;
    headdataavailable =  PTHREAD_COND_INITIALIZER;
    headwriteprotect = PTHREAD_MUTEX_INITIALIZER;
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_create(&worker, &attr, trampoline, this);
    running = true;
}

looper::~looper() {
    if (running) {
        printf("Looper deleted while still running. Some messages will not be processed\n");
        quit();
    }
}

void looper::post(int what, void *data, bool flush) {
    loopermessage *msg = new loopermessage();
    msg->what = what;
    msg->obj = data;
    msg->next = NULL;
    msg->quit = false;
    addmsg(msg, flush);
}

void looper::addmsg(loopermessage *msg, bool flush) {
    pthread_mutex_lock(&headwriteprotect);
    loopermessage *h = head;
    if (flush) {
        while(h) {  //清空链表
            loopermessage *next = h->next;
            delete h;
            h = next;
        }
        h = NULL;
    }
    if (h) {    //链表存入
        while (h->next) {
            h = h->next;
        }
        h->next = msg;
//        printf("+push head->next\n");
    } else {
//        printf("++push head\n");
        head = msg;
    }
    printf("post msg %d\n", msg->what);
    pthread_cond_signal(&headdataavailable);
    pthread_mutex_unlock(&headwriteprotect);
}

void looper::loop() {
    while(true) {
        pthread_mutex_lock(&headwriteprotect);
        if (head == NULL) {
            printf("no msg, waitting for\n");
            pthread_cond_wait(&headdataavailable,&headwriteprotect);
        }
        loopermessage *msg = head;
        head = msg->next;
//        if(head == NULL)
//            printf("after msg->next: head == NULL\n");

        if (msg->quit) {
            printf("quitting\n");
            delete msg;
            return;
        }
        printf("processing msg %d\n", msg->what);
//        handle(msg->what, msg->obj);
        delete msg;
        pthread_mutex_unlock(&headwriteprotect);
    }
}

void looper::quit() {
    printf("quit\n");
    loopermessage *msg = new loopermessage();
    msg->what = 0;
    msg->obj = NULL;
    msg->next = NULL;
    msg->quit = true;
    addmsg(msg, false);
    void *retval;
    pthread_join(worker, &retval);
    pthread_cond_destroy(&headdataavailable);
    running = false;
}

void looper::handle(int what, void* obj) {
    printf("dropping msg %d %s\n", what, obj);
}

int main(){
    looper loo;
    int i = 0;
    while(true){
        loo.post(i,(void *)"pass the data",0);
        loo.post(2,(void *)"pass the data",0);
//        loo.post(3,(void *)"pass the data",0);
        i++;
//        sleep(2);
    }
    while(1);
    return 0;
}
