#include "receiver.h"

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr){
    /*  TODO: 
        1. Use flag to determine the communication method
        2. According to the communication method, receive the message
    */
    if(mailbox_ptr -> flag == 1) mq_receive(mailbox_ptr -> storage.msqid, message_ptr -> message, MAX_MESSAGE_SIZE, 0);  // if flag is 1, use message queue to send msg
    if(mailbox_ptr -> flag == 2) strcpy(message_ptr -> message, mailbox_ptr -> storage.shm_addr);   //if flag is 2, use shared memory
}

int main(int argc, char* argv[]){
    /*  TODO: 
        1) Call receive(&message, &mailbox) according to the flow in slide 4
        2) Measure the total receiving time
        3) Get the mechanism from command line arguments
            • e.g. ./receiver 1
        4) Print information on the console according to the output format
        5) If the exit message is received, print the total receiving time and terminate the receiver.c
    */
    mailbox_t mailbox;
    mailbox.flag = mailbox.flag = (argv[1][0] == '1') ? 1 : 2;   // get the mechanism from command line arguments

    sem_t *recv_sem = sem_open("/recv_sem", 0);  //open semaphore
    sem_t *send_sem = sem_open("/send_sem", 0);

    if(mailbox.flag == 1){
        
        printf("Message Passing\n");
        mailbox.storage.msqid = mq_open("/msg_queue", O_RDONLY);   // open the message queue
    
    }else{

        printf("Shared Memory\n");
        int shared_mem_fd = shm_open("/share_memory", O_RDONLY, 0666);// open the shared memory
        mailbox.storage.shm_addr = mmap(NULL, sizeof(char) * MAX_MESSAGE_SIZE, PROT_READ, MAP_SHARED, shared_mem_fd, 0); // map the shared memory

    }

    struct timespec start, end;
    double time = 0;

    
    while (1)
    {  
        message_t recv;
        sem_wait(send_sem);
        clock_gettime(CLOCK_MONOTONIC, &start);
        receive(&recv, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        sem_post(recv_sem);
        time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
        if(strcmp(recv.message, "exit") == 0){
            printf("Sender exit!\n");// if the exit message is received, print the total receiving time and terminate the receiver.c
            sem_post(recv_sem);       // send the last signal to sender
            break;
        }
        printf("Receiving message: %s\n", recv.message);
    }

    printf("Total taken time in receiving message: %f s\n", time);


    sem_close(send_sem);// close the semaphore
    sem_close(recv_sem); // close the semaphore
    if(mailbox.flag == 1) mq_close(mailbox.storage.msqid);  // close the message queue
    if(mailbox.flag == 2) munmap(mailbox.storage.shm_addr, sizeof(char) * MAX_MESSAGE_SIZE);  // unmap the shared memory

    return 0;
}
