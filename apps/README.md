# Tiger® Applications Folder

## How to Use

1. Create a folder with the name of your application, like: my_app
1. In the folder, create a CMakeList.txt, and write: 
```cmake
ti_add_app(my_app MODULES module1 module2 EXTRAS ${qt_link} Boost::boost)
  
```
1. macro ti_add_app will search for your 

## TO BE CONTINUE