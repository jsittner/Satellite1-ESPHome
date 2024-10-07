#include "tasks_monitor.h"

#include "esphome/core/log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portable.h"

namespace esphome {
namespace runtime_testing {

static const char *TAG = "Task-Monitor";

void log_task_info(uint32_t coreID) {
    // Allocate space to hold task status
    const UBaseType_t numTasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *taskStatusArray = (TaskStatus_t *)pvPortMalloc(numTasks * sizeof(TaskStatus_t));

    if (taskStatusArray == NULL) {
        ESP_LOGD( TAG, "Failed to allocate memory for task status array\n");
        return;
    }

    // Get the system state (information about each task)
    UBaseType_t taskCount = uxTaskGetSystemState(taskStatusArray, numTasks, NULL);
    
    ESP_LOGD(TAG, "Tasks on Core %d\n", coreID);
    ESP_LOGD(TAG, "Task Name\t\tStack High Watermark (bytes remaining)\tTask State\n");
    ESP_LOGD(TAG, "-----------------------------------------------------------------\n");

    for (UBaseType_t i = 0; i < taskCount; i++) {
        UBaseType_t affinity = xTaskGetAffinity(taskStatusArray[i].xHandle);
        
        if (affinity == coreID || affinity == tskNO_AFFINITY) {
            const char *taskState = "";

            switch (taskStatusArray[i].eCurrentState) {
                case eRunning:   taskState = "Running"; break;
                case eReady:     taskState = "Ready"; break;
                case eBlocked:   taskState = "Blocked"; break;
                case eSuspended: taskState = "Suspended"; break;
                case eDeleted:   taskState = "Deleted"; break;
                default:         taskState = "Unknown"; break;
            }

            ESP_LOGD( TAG, "%s\t\t%u\t\t\t\t%s\n",
                   taskStatusArray[i].pcTaskName,
                   taskStatusArray[i].usStackHighWaterMark * sizeof(StackType_t), // Convert to bytes
                   taskState);
        }
    }

    // Free allocated memory
    vPortFree(taskStatusArray);
}



void TaskMonitor::setup(){

}

void TaskMonitor::loop(){
    log_task_info(0);
    log_task_info(1);
    vTaskDelay(pdMS_TO_TICKS(5000));
}

}
}