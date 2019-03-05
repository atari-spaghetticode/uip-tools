
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

        var CURRENT_GEMDOS_PATH=null;
        var FILE_LIST_REF=null;
        var FILE_VIEW_REF=null;
        var CURRENT_PATH_INPUT_REF=null;
        var DIR_LIST_REF=null;
        var DIR_LIST_VIEW_REF=null;
        var DRIVE_BUTTON_LIST_TAB_REF=null;
        var DRAGNDROP_AREA_REF=null;
        var FILE_UPLOAD_PROGRESSBAR=null;
        var DEBUG_OUTPUT_REF=null;

        function $id(id) {
          return document.getElementById(id);
        }

        function dnd_preventDefaults (e) {
          e.preventDefault()
          e.stopPropagation()
        }

        function dnd_highlight(e) {
          DRAGNDROP_AREA_REF.classList.add('highlight')
        }

        function dnd_unhighlight(e) {
          DRAGNDROP_AREA_REF.classList.remove('active')
        }

        function dnd_handleDrop(e) {
          var dt = e.dataTransfer
          var files = dt.files
          uploadFiles(files)
        }

        // progress bar handling
        var uploadProgress = [];

        function initializeProgress(numFiles) {
          
          FILE_UPLOAD_PROGRESSBAR.value = 0;
          uploadProgress = [];
          
          for(var i = 0; i < numFiles; ++i) {
            uploadProgress.push(0);
          }
        }

        function sum(total, value){
          return total + value;
        }
        
        function updateProgress(fileNumber, percent) {
          uploadProgress[fileNumber] = percent;
          var total = (uploadProgress.reduce(sum,0) / uploadProgress.length);
          FILE_UPLOAD_PROGRESSBAR.value = total;
        }


        function convertFileNameToGemdos(filename){
          return filename;
        }

        function handleUploadError(e){
          if(this.readyState==4){
            if(this.status == 200 || this.status == 201){
               // Done. Inform the user
              console.log("Success: upload done."); 
               //TODO: refresh dir ui 

              return;            
            }else{
                // Error. Inform the user
                console.log("Error: upload failed." + this.responseText);  
            }
         }
        }

        function uploadFiles(files) {
          var fileArray = Array.from(files);
          initializeProgress(fileArray.length); 
 
          var gemdosName = null;
          
          for (var i = 0; i < fileArray.length; ++i) {
            // upload file request
            // request = localpath + "/" + path + name + "?setfiledate=" + convertDateToAtariTOSFormat(date)
            gemdosName = convertFileNameToGemdos(fileArray[i].name);

            var request = CURRENT_PATH_INPUT_REF.value + '/' + gemdosName;
            request = sanitizeGemdosPath(request);
            
            var current_date = new Date();
            request = request+"?setfiledate=" + convertDateToAtariTOSFormat(current_date);
            
            var reader = new FileReader();
            reader.onload = (function(request) {
            
            return function(event) {
                var xhr = new XMLHttpRequest;
                var blob = new Blob([event.target.result], {type: 'application/octet-binary'});
                
                xhr.onreadystatechange = handleUploadError;
                xhr.upload.onprogress= (function onUpdateProgressBar(e) {
                   updateProgress(i, (e.loaded * 100.0 / e.total) || 100)
                });
                
                xhr.open('POST', request, true);
                xhr.send(blob);
              };
            })(request);

            reader.readAsArrayBuffer(fileArray[i]);
          }
        }

        function initDragAndDropControl(){
            // Prevent default drag behaviors
            DRAGNDROP_AREA_REF.addEventListener('dragenter', dnd_preventDefaults, false)   
            DRAGNDROP_AREA_REF.addEventListener('dragover', dnd_preventDefaults, false)   
            DRAGNDROP_AREA_REF.addEventListener('dragleave', dnd_preventDefaults, false)   
            DRAGNDROP_AREA_REF.addEventListener('drop', dnd_preventDefaults, false)   
            
            document.body.addEventListener('dragenter', dnd_preventDefaults, false);
            document.body.addEventListener('dragover', dnd_preventDefaults, false);
            document.body.addEventListener('dragleave', dnd_preventDefaults, false);
            document.body.addEventListener('drop', dnd_preventDefaults, false);

            // Highlight drop area when item is dragged over it
            DRAGNDROP_AREA_REF.addEventListener('dragenter', dnd_highlight, false);
            DRAGNDROP_AREA_REF.addEventListener('dragover', dnd_highlight, false);
            DRAGNDROP_AREA_REF.addEventListener('dragleave', dnd_unhighlight, false);
            DRAGNDROP_AREA_REF.addEventListener('drop', dnd_unhighlight, false);

            // Handle dropped files
            DRAGNDROP_AREA_REF.addEventListener('drop', dnd_handleDrop, false);
        }

        function initGlobalReferences(){
            CURRENT_GEMDOS_PATH = "";
            DIR_LIST_REF = $id("directoryList");
            FILE_LIST_REF = $id("fileList");
            FILE_VIEW_REF = $id("fileView");
            DRAGNDROP_AREA_REF = $id("fileDragAndDrop");
            CURRENT_PATH_INPUT_REF = $id("currentPathInput");
            DIR_LIST_VIEW_REF = $id("directoryListView");
            DRIVE_BUTTON_LIST_TAB_REF = $id("driveButtonListTab");
            FILE_UPLOAD_PROGRESSBAR = $id("progressBar");
            DEBUG_OUTPUT_REF = $id("debugOutput");
        }

        function initFavicon(){
          var docHead = document.getElementsByTagName('head')[0];       
          var newLink = document.createElement('link');
          newLink.rel = 'shortcut icon';
          newLink.href = 'data:image/png;base64,' + img_favicon_src;
          docHead.appendChild(newLink);
        }

        function sanitizeGemdosPath(gemdosPath){
          var path = gemdosPath;
          path = path.replace(":",'/');
          path = path.replace(/\/+/g, '/').replace(/\/+$/, ''); 
          path = path.replace(/\\\\/g, '\\');
          path = path.replace(/\/\//g,'/');
          return path;
        }

        // view cleanup on reload
        function initMainView(){

          initGlobalReferences();
          initFavicon();
          initDragAndDropControl();

          if(DIR_LIST_REF!=null){
            DIR_LIST_REF.parentNode.removeChild(DIR_LIST_REF);
            DIR_LIST_REF=null;
          }
          
          if(FILE_LIST_REF!=null){
            FILE_LIST_REF.parentNode.removeChild(FILE_LIST_REF);
            FILE_LIST_REF=null;
          }

          CURRENT_PATH_INPUT_REF.value = CURRENT_GEMDOS_PATH;
        }

        function processDirectoryListReq(responseText){
          var DirListArray = JSON.parse(responseText);

          updateDirectoryViewUI(DirListArray);
          updateFileViewUI(DirListArray);
        }

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

        function requestChDrive(btn){
    
            // request dir status
            var dirListJsonResult = sendHttpReq(location.host + '/' + btn.name,'dir', 'GET', true, processDirectoryListReq);
            
            //todo: check request result
            CURRENT_GEMDOS_PATH = btn.name + ':'; 
            CURRENT_PATH_INPUT_REF.value = CURRENT_GEMDOS_PATH;
        }

        function requestChDir(btn){
            var pathPrefix='';

            if(btn.name=='ROOT'){

              var pathArray = CURRENT_GEMDOS_PATH.replace(/\/$/, "").split('/');
              
              CURRENT_GEMDOS_PATH='';
              
              for(var i=0;i<(pathArray.length-1);++i){
                CURRENT_GEMDOS_PATH = CURRENT_GEMDOS_PATH + pathArray[i] + '/' ;
              }

              pathPrefix = sanitizeGemdosPath(CURRENT_GEMDOS_PATH);

              //request dir status
              var dirListJsonResult = sendHttpReq(location.host + '/' + pathPrefix ,'dir', 'GET', true, processDirectoryListReq);

              //todo: check request result
              CURRENT_PATH_INPUT_REF.value = '';
              CURRENT_PATH_INPUT_REF.value = CURRENT_GEMDOS_PATH.replace(/\/$/, "").replace(/\/\//g,'/');;

            }else{
                          
              pathPrefix = sanitizeGemdosPath(CURRENT_GEMDOS_PATH);

              //request dir status
              var dirListJsonResult = sendHttpReq(location.host + '/' + pathPrefix + '/'+ btn.name,'dir', 'GET', true, processDirectoryListReq);

              //todo: check request result
              CURRENT_GEMDOS_PATH = CURRENT_GEMDOS_PATH + '/' + btn.name;
              CURRENT_PATH_INPUT_REF.value = CURRENT_GEMDOS_PATH;
            }
            
        }

        function requestFileDownload(btn){
            
            var pathPrefix = CURRENT_GEMDOS_PATH;
            pathPrefix = pathPrefix + '/' + btn.name;
            pathPrefix = sanitizeGemdosPath(pathPrefix);
 
            // request file download
            var result = sendHttpReq(location.host + '/' + pathPrefix,'', 'GET', true, processFileDownloadReq);
        } 


        function handleDriveOnClick(){
          requestChDrive(this);            
          return false;
        }

        function createDriveButtons(node, DriveArray){
          var buttonStr = null;
          
          for(var i=0;i<DriveArray.length;++i){

             buttonStr = DriveArray[i].name.toUpperCase();
             var button = document.createElement("button");
             var textNode = document.createTextNode(buttonStr + ':');

             var img = document.createElement('img');
 
             if(buttonStr=='A' || buttonStr=='B'){
                img.alt = "file icon floppy drive";
                img.src = 'data:image/png;base64,' + img_floppy_active_src;
             } else {
                img.alt = "file icon hard drive";
                img.src = 'data:image/png;base64,' + img_hd_active_src;
             }
 
             node.appendChild(img);
             button.appendChild(img);
             
             button.appendChild(textNode);
             button.type='button';
             button.name = buttonStr;
             button.onclick = handleDriveOnClick;
             node.appendChild(button);
          };

        }

        function updateDriveViewUI(DriveArray){
          var div = document.createElement("div");
          div.id="DriveButtonGroup"
          div.innerHTML = '';
          createDriveButtons(div, DriveArray);            
          
          DRIVE_BUTTON_LIST_TAB_REF.appendChild(div);
        }

        // directory view generation
        function handleDirectoryOnClick(){
            requestChDir(this);
            return false;
        }

        function createDirectoryEntries(node, DirectoryArray){
          var directoryStr = null;
          var directoryReqStr=null;
          var entriesFound=0;

          if(CURRENT_GEMDOS_PATH.length > 2){
            var rootbtn = document.createElement("button");
            var rootbtnNode = document.createTextNode('..');
                
            rootbtn.appendChild(rootbtnNode);
            rootbtn.type='button';
            rootbtn.name = 'ROOT';
            rootbtn.onclick = handleDirectoryOnClick;
            node.appendChild(rootbtn);
          }

          for(var i=0;i<DirectoryArray.length;++i){
             if(DirectoryArray[i].type.toLowerCase()=='d'){
                ++entriesFound;
                directoryStr = DirectoryArray[i].name.toUpperCase();
                directoryReqStr=directoryStr;
                if(directoryStr=='ROOT') directoryStr='..';
                if(directoryReqStr=='ROOT') directoryReqStr='';
                
                var button = document.createElement("button");
                var textNode = document.createTextNode(directoryStr);
                var requestStr = directoryReqStr;

                var img = document.createElement('img');
                img.alt = "folder closed icon";
                img.src = 'data:image/png;base64,' + img_dir_close_src;
                button.appendChild(img);

                button.appendChild(textNode);
                button.type='button';
                button.name = requestStr;
                button.onclick = handleDirectoryOnClick;
                button.style="display:block;";
                node.appendChild(button);
           }
          };
           
           if(entriesFound==0) node.innerHTML+='[Empty]<br/>' ;
                 
        }

        function updateDirectoryViewUI(DirJsonArray){

          var div = document.createElement("div");
          div.id="directoryList";
          createDirectoryEntries(div,DirJsonArray);

          if(DIR_LIST_REF!=null){
            DIR_LIST_REF.parentNode.removeChild(DIR_LIST_REF);
            DIR_LIST_REF=null;
          }

          DIR_LIST_VIEW_REF.appendChild(div);
          DIR_LIST_REF=$id("directoryList");
        }

        // file view generation

        function handleFileOnClick(){
            requestFileDownload(this);
            return false;            
        }

        function createFileDownloadLink(node, fileName){
            var a = document.createElement('a');
            var linkText = document.createTextNode(fileName);
            a.appendChild(linkText);

            var pathPrefix = CURRENT_GEMDOS_PATH;
            pathPrefix += '/' + fileName;
            
            pathPrefix = sanitizeGemdosPath(pathPrefix);
            
            a.href = pathPrefix;  
            a.download = fileName;

            node.appendChild(a);
        }

        function createFileEntries(node, FileArray){
          var fileStr = null;
          var fileSize=0;
          var fileDate="";

          var entriesFound=0;
          
          var tableDiv = document.createElement('div');
          tableDiv.classList.add('divTable');
          tableDiv.classList.add('blueTable');
          
          var tableHead = document.createElement('div');
          tableHead.classList.add('divTableHeading');
         
          var tableRow = document.createElement('div');
          tableRow.classList.add('divTableRow');
          
          var tableHead1 = document.createElement('div');
          var tableHead2 = document.createElement('div');
          var tableHead3 = document.createElement('div');

          tableHead1.classList.add('divTableHead');
          tableHead2.classList.add('divTableHead');
          tableHead3.classList.add('divTableHead');

          var headNameText1=document.createTextNode('filename');
          var headNameText2=document.createTextNode('size');
          var headNameText3=document.createTextNode('date');
 
          tableHead1.appendChild(headNameText1);
          tableHead2.appendChild(headNameText2);
          tableHead3.appendChild(headNameText3);

          tableRow.appendChild(tableHead1);
          tableRow.appendChild(tableHead2);
          tableRow.appendChild(tableHead3);

          tableHead.appendChild(tableRow);
          tableDiv.appendChild(tableHead);

          var tableBody = document.createElement('div');
          tableBody.classList.add('divTableBody');
          tableDiv.appendChild(tableBody);
          /* add entries to tableBody */
          node.appendChild(tableDiv);

          for(var i=0;i<FileArray.length;++i){
           
            if(FileArray[i].type.toLowerCase()=='f'){
              ++entriesFound;
             
             var fileRow = document.createElement('div');
             fileRow.classList.add('divTableRow');

             var cell1 = document.createElement('div');
             var cell2 = document.createElement('div');
             var cell3 = document.createElement('div');
             
             cell1.classList.add('divTableCell');
             cell2.classList.add('divTableCell');
             cell3.classList.add('divTableCell');
             
             fileRow.appendChild(cell1);
             fileRow.appendChild(cell2);
             fileRow.appendChild(cell3);

             tableBody.appendChild(fileRow);
             
             fileStr = FileArray[i].name.toUpperCase();
             fileSize = FileArray[i].size;
             fileDate = FileArray[i].date;

             var div = document.createElement('div');
             div.id = "fileEntryInfo";

             var img = document.createElement('img');
             img.alt = "file icon generic";
             img.src = 'data:image/png;base64,' + img_file_generic_src;
             div.appendChild(img);

             cell1.appendChild(div);
             cell2.innerHTML = fileSize;
             cell3.innerHTML = fileDate;
             
             createFileDownloadLink(div,fileStr);
             
          };
        }
            if(entriesFound==0) 
              node.innerHTML += '[EMPTY]<br/>';
        }


        function updateFileViewUI(FileJsonArray){

          var div = document.createElement("div");
          div.id="fileList"
          createFileEntries(div, FileJsonArray)

          if(FILE_LIST_REF!=null){
            FILE_LIST_REF.parentNode.removeChild(FILE_LIST_REF);
            FILE_LIST_REF=null;
          }

          FILE_VIEW_REF.appendChild(div);
          FILE_LIST_REF=$id("fileList");
        }


        function processDriveListReq(responseText) {
          var DriveArray = JSON.parse(responseText);
          updateDriveViewUI(DriveArray);  
        }


        // send http request 
        function sendHttpReq(addr, params, requestType, isAsync, requestHandler){

            var xhr = new XMLHttpRequest();
            
            if(params==null || params.length==0){
              params = '';
            } else{
              params = '/?' + params;
            }

            if(isAsync!=true){
                xhr.open(requestType, 'http://' + addr + params, isAsync);
                xhr.send();
                return xhr.responseText; 
            }else{
                xhr.onreadystatechange = function() { 
                    if (xhr.readyState == 4 && xhr.status == 200)
                       requestHandler(xhr.responseText);
                    }

                xhr.open(requestType, 'http://' + addr + params, isAsync);
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

     // debug output functions
     function DebugOutput(msg) {
        var m = DEBUG_OUTPUT_REF;
        m.innerHTML = msg + m.innerHTML;
    }
