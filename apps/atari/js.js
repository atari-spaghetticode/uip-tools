// <!-- this will be gone after http interfaces resurrection -->
        var driveDummyData = [];         
        var directoryDummyData = []; 
        var fileDummyData = []; 

        function initSampleData(){
            // sample drive data

            driveDummyData.push({ name: 'A', type: 'd' });
            driveDummyData.push({ name: 'B', type: 'd' });
            driveDummyData.push({ name: 'C', type: 'd' });
            driveDummyData.push({ name: 'D', type: 'd' });
            driveDummyData.push({ name: 'E', type: 'd' });
            driveDummyData.push({ name: 'F', type: 'd' });
            driveDummyData.push({ name: 'G', type: 'd' });
            driveDummyData.push({ name: 'H', type: 'd' });
            driveDummyData.push({ name: 'I', type: 'd' });
            driveDummyData.push({ name: 'J', type: 'd' });
            driveDummyData.push({ name: 'K', type: 'd' });
            driveDummyData.push({ name: 'L', type: 'd' });
            driveDummyData.push({ name: 'M', type: 'd' });
            driveDummyData.push({ name: 'N', type: 'd' });
            driveDummyData.push({ name: 'O', type: 'd' });
            driveDummyData.push({ name: 'P', type: 'd' });

            // sample current dir data
            directoryDummyData.push({ name: 'root', type: 'd' });
            directoryDummyData.push({ name: 'AUTO', type: 'd' });
            directoryDummyData.push({ name: 'NVDI', type: 'd' });
            directoryDummyData.push({ name: 'CPX', type: 'd' });
            directoryDummyData.push({ name: 'NEW.FLD', type: 'd' });

            // sample file data
            fileDummyData.push({ name: 'DESKTOP.INF', type: 'f', date: '29-03-80', size: 12234 });
            fileDummyData.push({ name: 'ZENON.ACC', type: 'f', date: '29-03-80', size: 3434 });
            fileDummyData.push({ name: 'DEMO.TOS', type: 'f', date: '29-03-80', size: 32000 });
            fileDummyData.push({ name: 'PIC.NeO', type: 'f', date: '29-03-80', size: 32000 });
            fileDummyData.push({ name: 'README.TXT', type: 'f', date: '29-03-80', size: 1312322 });
        }
// <!-- ----------------------------------------------------------------------------------------- -->

        function processDirectoryListReq(responseText){
          var DirListArray = JSON.parse(responseText);

          updateDirectoryViewUI(DirListArray);
          updateFileViewUI(DirListArray);
        }

        function requestChangeCurrentDirectory(btn){
            //request dir status
            var dirListJsonResult = sendHttpReq(location.host + '/' + btn.name + '/','dir', 'GET', true, processDirectoryListReq);

          //update current path input string
            var InputElem = document.getElementById("currentPathInput");
            // TOS drives: A/B(floppy), C-P
            // MultiTOS: A/B,C-Z + U 
            // files/directories 8+3
            // valid GEMDOS characters:  
            // A-Z, a-z, 0-9
            // ! @ # $ % ^ & ( )
            // + - = ~ ` ; ' " ,
            // < > | [ ] ( ) _ 
            // . - current dir
            // .. parent dir

            if(InputElem!=null){
                InputElem.value=btn.name+':'+'\\';
            }else{
                 InputElem.value='';
            }  
            
        }

        function requestChDir(source){

        }

        function requestFileAction(source){

        } 

        function createDriveButtons(node, DriveArray){
          var buttonStr = null;
          
          for(var i=0;i<DriveArray.length;++i){

             buttonStr = DriveArray[i].name.toUpperCase();
             var button = document.createElement("button");
             var textNode = document.createTextNode(buttonStr + ':');
             button.appendChild(textNode);
             button.type='button';
             button.name = buttonStr;
             
             button.onclick = function(){
                requestChangeCurrentDirectory(this);            
             }

             node.appendChild(button);
          };

        }

        function updateDriveViewUI(DriveArray){

          var div = document.createElement("div");
          div.id="DriveButtonGroup"
          div.innerHTML = '';
          createDriveButtons(div, DriveArray);            
          var elem = document.getElementById("DriveButtonListTab");
          elem.appendChild(div);
        }

        // directory view generation
        function createDirectoryEntries(node, DirectoryArray){
          var directoryStr = null;
          var entriesFound=0;

          for(var i=0;i<DirectoryArray.length;++i){
             if(DirectoryArray[i].type.toLowerCase()=='d'){
                ++entriesFound;
                directoryStr = DirectoryArray[i].name.toUpperCase();
                if(directoryStr=='ROOT') directoryStr='..';

                node.innerHTML += directoryStr + '<br/>'
                node.onclick = function(){
                requestChDir(this);            
             }
           }
          };
            if(entriesFound>0) node.innerHTML+='<br/>'
            else node.innerHTML+='No directories found!<br/>'       
        }

        function updateDirectoryViewUI(DirJsonArray){
        
          var div = document.createElement("div");
          div.id="DirectoryList";
          div.innerHTML = '<br/>';

          createDirectoryEntries(div,DirJsonArray);
          var elem = document.getElementById("directoryListView");
          var oldElem = document.getElementById("DirectoryList");
          
          if(oldElem!=null){
            oldElem.parentNode.removeChild(oldElem);
          }

          elem.appendChild(div);
        }

        // file view generation 
        function createFileEntries(node, FileArray){
          var fileStr = null;
          var entriesFound=0;
          for(var i=0;i<FileArray.length;++i){
           
            if(FileArray[i].type.toLowerCase()=='f'){
             ++entriesFound;
             fileStr = FileArray[i].name.toUpperCase();
             node.innerHTML += fileStr + '<br/>'
           }

          };
            if(entriesFound>0) node.innerHTML+='<br/>';
              else node.innerHTML+='No files found!<br/>';
        }


        function updateFileViewUI(FileJsonArray){

          var div = document.createElement("div");
          div.id="FileList"
          div.innerHTML = '<br/><br/>';

          createFileEntries(div, FileJsonArray)
          
          var elem = document.getElementById("fileView");
          var oldElem = document.getElementById("FileList");
          
          if(oldElem!=null){
            oldElem.parentNode.removeChild(oldElem);
          }

          elem.appendChild(div);
        }


        function processDriveListReq(responseText) {
          var DriveArray = JSON.parse(responseText);
          updateDriveViewUI(DriveArray);  
        }


        // send http request 
        function sendHttpReq(addr, params, requestType, isAsync, requestHandler){

            var xhr = new XMLHttpRequest();
            
            if(isAsync!=true){
                xhr.open(requestType, 'http://' + addr +'/?' + params, isAsync);
                xhr.send();
                return xhr.responseText; 
            }else{
                xhr.onreadystatechange = function() { 
                    if (xhr.readyState == 4 && xhr.status == 200)
                       requestHandler(xhr.responseText);
                    }

                xhr.open(requestType, 'http://' + addr +'/?' + params, isAsync);
                xhr.send();
            }

            return null;
        }

        function updateDriveListReq(){
            var driveListJsonResult = sendHttpReq(location.host,'dir', 'GET', true, processDriveListReq);
            
        }


    // This returns at least 32bit int with date encoded for DOSTIME struct
    // typedef struct
    // {
    //      uint16_t     time;  /* Time like Tgettime */
    //      uint16_t     date;  /* Date like Tgetdate */
    // } DOSTIME;
    
    function convertDateToAtariTOSFormat(date) {
        var tosYear = (date.getFullYear() - 1980);
        tosYear = tosYear < 0 ? 0 : tosYear;
        tosYear = tosYear > 119 ? 119 : tosYear;
        var tosDay = date.getDate();
        var tosMonth = date.getMonth() + 1;
        var tosDate = (tosYear) << 9 | (tosMonth << 5) | tosDay;
        var tosSeconds = date.getSeconds() / 2;
        var tosMinutes = date.getMinutes();
        var tosHours = date.getHours();
        var tosTime = (tosHours << 11) | (tosMinutes<<5) | tosSeconds;
        return ((tosTime<<16) | tosDate) >>> 0;
     }

    function openLogTab(evt, tabName) {
        var i, tabcontent, tablinks;

        tabcontent = document.getElementsByClassName("tabcontent");
        tablinks = document.getElementsByClassName("tablinks");
        buttoncontent = document.getElementsByClassName("button");
        
        // Get all elements with class="tabcontent" and hide them
        for (i = 0; i < tabcontent.length; ++i) {
            tabcontent[i].style.display = "none";
        }

        // Get all elements with class="tablinks" and remove the class "active"
        for (i = 0; i < tablinks.length; ++i) {
            tablinks[i].className = tablinks[i].className.replace(" active", "");
        }

       for (i = 0; i < buttoncontent.length; ++i) {
            buttoncontent[i].style.visibility = 'hidden';
        }

        // Show the current tab, and add an "active" class to the button that opened the tab
        var elem=document.getElementById(tabName)
        
        if(elem!=null){
            elem.style.display = "block";
        }

        elem = document.getElementById("Clear" + tabName + "Button")
        if(elem!=null){
          elem.style.visibility = 'visible';       
          evt.currentTarget.className += " active";
        }
        
    } 