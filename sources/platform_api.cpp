#include "stdio.h"
#include "linux_types.h"
#include <errno.h>
#include <stdlib.h>
#include <malloc.h>
#include "string.h"
#include <gtk/gtk.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <dlfcn.h>

#include "util_time.h"
#include "linux_time.cpp"


bool * gassert;
char * gassertMessage;

#define ASSERT(expression) if(!(expression)) { sprintf(gassertMessage, "ASSERT FAILED on line %d in file %s\n", __LINE__, __FILE__); *gassert = true; printf(gassertMessage);}


#include "common.h"

#define PERSISTENT_MEM MEGABYTE(10)
#define TEMP_MEM MEGABYTE(10)
#define STACK_MEM MEGABYTE(10)


#include "util_mem.h"
#include "util_filesystem.h"
#include "linux_filesystem.cpp"

#include "util_font.cpp"
#include "util_image.cpp"

#include <wiringPiI2C.h>


#include "domaincode.h"

struct PlatformState{
    GtkWindow * window;
    GtkWidget * drawArea;
    GdkPixbuf * pixbuf;
    GByteArray bytearray;
    uint32 frame;
    uint32 lastFrameMark;
    float32 accumulator;
    bool validInit;
};


char domainDll[] = "./domain.so";

long lastChange = 0;
void * domainHandle = NULL;
void (*initDomain)(bool*,char*,void*) = NULL;
void (*iterateDomain)(bool*) = NULL;
void (*closeDomain)(void) = NULL;

PlatformState * platformState;



inline void closeDll(){
    if(domainHandle != NULL){
        if(closeDomain != NULL) closeDomain();
        dlclose(domainHandle);
        domainHandle = NULL;
        initDomain = NULL;
        closeDomain = NULL;
        iterateDomain = NULL;
    }
}

inline bool hotloadDomain(){
    struct stat attr;
    stat(domainDll, &attr);
    
    if(lastChange != (long)attr.st_mtime){
        printf("Domaincode changed\n");
        closeDll();
        *gassert = false;
        domainHandle = dlopen(domainDll, RTLD_NOW | RTLD_GLOBAL);
        if(domainHandle){
            initDomain = (void (*)(bool*,char*,void*))dlsym(domainHandle, "initDomain");
            iterateDomain = (void (*)(bool*))dlsym(domainHandle, "iterateDomain");
            closeDomain = (void (*)(void))dlsym(domainHandle, "closeDomain");
            if(closeDomain == NULL || iterateDomain == NULL || closeDomain == NULL){
                printf("Error: Failed to find functions\n");
                dlclose(domainHandle);
                domainHandle = NULL;
                return false;
            }else{
                initDomain(gassert, gassertMessage, (void*)(platformState+1));
            }
            
        }else{
            printf("dlerr: %s\n", dlerror());
            return false;
        }
        
        
        lastChange = (long)attr.st_mtime;
    }
    return true;
}

DomainInterface * domainInterface;


gboolean
draw(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    gdk_draw_pixbuf(widget->window, NULL, platformState->pixbuf, 0, 0, 0, 0, domainInterface->renderTarget.info.width, domainInterface->renderTarget.info.height, GDK_RGB_DITHER_NONE, 0, 0);
    
    
    return TRUE;
}



extern "C" void initPlatform(bool * assert, char * assertMessage)
{
    gassertMessage = assertMessage;
    gassert = assert;
    lastChange = 0;
    
    
    
    void * memoryStart = valloc(TEMP_MEM + PERSISTENT_MEM + STACK_MEM);
    if (memoryStart)
    {
        initMemory(memoryStart);
        
        platformState = (PlatformState *) mem.persistent;
        ASSERT(PERSISTENT_MEM >= sizeof(PlatformState) + sizeof(DomainInterface));
        platformState->validInit = false;
        bool hotload = hotloadDomain();
        if(hotload)
        {
            gtk_init(NULL, NULL);
            
            
            platformState->window = (GtkWindow *)gtk_window_new(GTK_WINDOW_TOPLEVEL);
            
            
            bool window = platformState->window != NULL;
            
            
            
            if(window){
                
                gtk_window_set_screen(platformState->window, gdk_screen_get_default());
                gtk_window_maximize(platformState->window);
                platformState->drawArea = gtk_drawing_area_new();
                gtk_container_add((GtkContainer * )platformState->window, platformState->drawArea);
                g_signal_connect (G_OBJECT (platformState->drawArea), "expose_event",
                                  G_CALLBACK (draw), NULL);
                gtk_widget_show(platformState->drawArea);
                gtk_window_set_decorated(platformState->window, false);
                gtk_widget_show_now((GtkWidget *)platformState->window);
                gint w;
                gint h;
                gtk_window_get_size(platformState->window, &w, &h);
                domainInterface->renderTarget.info.width = w;
                domainInterface->renderTarget.info.height = h;
                domainInterface->renderTarget.info.origin = BitmapOriginType_TopLeft;
                domainInterface->renderTarget.info.interpretation = BitmapInterpretationType_ABGR;
                domainInterface->renderTarget.info.bitsPerSample = 8;
                domainInterface->renderTarget.info.samplesPerPixel = 4;
                
                
                platformState->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, w, h);
                domainInterface->renderTarget.data = gdk_pixbuf_get_pixels(platformState->pixbuf);
                
                platformState->accumulator = 0;
                platformState->frame = 0;
                domainInterface->lastFps = 0;
                platformState->validInit = true;
                
                printf("Valid platform init\n");
                
            }else{
                printf("invalid init\n");
                
                if(!window){
                    printf("Window init err\n");
                }
                
                printf("errno: %d\n", errno);
            }
            
            
            
        }
    }
}

extern "C" void iterate(bool * keepRunning){
    
    if(platformState->validInit)
    {
        
        hotloadDomain();
        //printf("platform iterate\n");
        if(domainHandle != NULL)
        {
            
            float32 startTime = getProcessCurrentTime();
            if(platformState->accumulator >= 1){
                uint32 seconds = (uint32)platformState->accumulator;
                platformState->accumulator = platformState->accumulator - seconds;
                platformState->lastFrameMark = platformState->frame;
            }else{
                domainInterface->lastFps = (uint32)((platformState->frame - platformState->lastFrameMark) / platformState->accumulator);
            }
            
#if 0
            
            GdkEvent * event;
            
            while((event = gtk_get_current_event()) != NULL){
                
                
                gdk_event_free(event);
            }
            
#endif
            gtk_main_iteration_do(false);
            //https://www.linuxtv.org/downloads/legacy/video4linux/API/V4L2_API/spec-single/v4l2.html#camera-controls
            
            
            
            
            
            
            //set renderer dimensions
            gint width;
            gint height;
            gtk_window_get_size(platformState->window, &width, &height);
            gtk_widget_set_size_request(platformState->drawArea, width, height);
            domainInterface->renderTarget.info.width = width;
            domainInterface->renderTarget.info.height = height;
            
            
            
            //do the domain code iteration
            //printf("iterating domain \n");
            iterateDomain(keepRunning);
            
            //call draw
            
            gtk_widget_queue_draw((GtkWidget *)platformState->window);
            gdk_window_process_all_updates();
            platformState->frame++;
            platformState->accumulator += getProcessCurrentTime() - startTime;
        }
        else
        {
            sleep(1);
        }
    }else{
        sleep(1);
        
    }
}

extern "C" void closePlatform(){
    printf("calling close\n");
    if(mem.persistent != NULL){
        if(platformState->window != NULL){
            printf("destroying window\n");
            //gtk_widget_destroy((GtkWidget *)platformState->drawArea);
            gtk_widget_destroy((GtkWidget *)platformState->window);
        }
        
        
        
        closeDll();
        free(mem.persistent);
        
    }
    
    
    
}




