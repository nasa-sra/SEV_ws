This is the full SEV ROS2 Jazzy Workspace for backup cameras, autonomous functionality and camera perception. 

To launch the container, run ```docker build -t sev_dev_image .
``` to build the container, and ```docker run -it --rm \ --name sev_dev_container \ -v "$(pwd)":/SEV_ws \ sev_dev_image``` to run the container. When running the container, it will update and upgrade all the packages in the container, this requires internet connection and may take a while. 
Along with that, if the output from ```ros wtf``` does not say ```All checks passed``` then there is a problem and it will not work.

After, you will see the terminal, where you are in the workspace.
It is recommended to use the VSCode DevContainers to open the docker container in it's own workspace and window. It makes it significantly easier to develop and run applications through.