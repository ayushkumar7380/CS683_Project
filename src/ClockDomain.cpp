#include "ClockDomain.h"

using namespace std;

namespace ClockDomain
{
                                     
ClockDomainCrosser::ClockDomainCrosser(ClockUpdateCB* _callback)
    : callback(_callback), clock1(1UL), clock2(1UL), counter1(0UL), counter2(0UL)
{
}
  
                                                                          
                         
                                                                             
           
 
                                                                             
                                        
 

                                                                              
                                                       
 
                                                                               
            
                         

                                
                 
                                            
                            
                  
                  
                  
                       

                                      
         
                                                                    
                 
                                                                
                              
                 
                                                                              
    

                                                                               
       
                                                            
                                       
                                             
                                                                         
                                     
                                                                            
         

                                                       
                           
                           

                                                                             
                                        
 
  
void ClockDomainCrosser::update()
{
                                        
    if (clock1 == clock2 && callback)
    {
        (*callback)();
        return;
    }

                        
    counter1 += clock1;

    while (counter2 < counter1)
    {
        counter2 += clock2;
                                                                                            
                                                             
        if (callback)
        {
                                                                                      
                              
            (*callback)();
        }
    }

    if (counter1 == counter2)
    {
        counter1 = 0;
        counter2 = 0;
    }
}

  

                  
 
                                        
 

                   
 
                                                                   
              

                                        
                                         
                                           
                                           
                                                                       
                                            
                                                              
                                               
                                                              
                                            
                                                              


                                
         

                           
                                                                               
                           
         

                 
 
  
}                          
