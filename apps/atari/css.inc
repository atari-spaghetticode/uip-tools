    <style type="text/css">
     
     html, body {
            height: 100%;
            width: 100%;
            margin: 0px;
            padding: 0px;
            overflow: hidden;
            clear:both;
      }
 
    nav{
        background-color: black;
        color: white;
        width:100%;
        border: 0px;
        margin-right:10px;
    }   

    footer {
        width:100%;
        float:none;
        background:black;
        color:white;    
        border:0px;
        clear:both;
    }

    input#currentPathInput{
        width:200px;
    }

    input#currentPathInput.pathInputControl{
        width:20%;
        overflow:hidden;
        border:0px;
        margin:0px;
    }

    div#CurrentDirPath{
        width:200px;
        border:0px;
        margin:0px;
    }

    div#DriveButtonGroup{
        width:100%;
    };


    div#mainSection{
        width:100%;
        height:80%;
    }


    div#fileManagementBlock{
        width:100%;
        height:80%;
    }

    div#directoryListView{
        width:20%;
    }

    div#DirectoryList{
     width : "20%";
     height : "80%";
     background-color: pink;
     color : black;
    };

    div#DriveButtonListTab{
        width:80%;
        float:right;
        overflow:hidden;
    }

   .directoryView {
            height: 80%;
            width: 200px;
            margin: 0px;
            padding: 0px;
            overflow: hidden;
   }


    <!-- log tab style -->
    /* Style the tab */
    .tab {
        overflow: hidden;
        border: 1px solid #ccc;
        background-color: #f1f1f1;
        width:100%;
        clear:both;
    }

    /* Style the buttons that are used to open the tab content */
    .tab button {
        background-color: inherit;
        float: left;
        border: none;
        outline: none;
        cursor: pointer;
        padding: 14px 16px;
        transition: 0.3s;
    }

    /* Change background color of buttons on hover */
    .tab button:hover {
        background-color: #ddd;
    }

    /* Create an active/current tablink class */
    .tab button.active {
        background-color: #ccc;
    }

    /* Style the tab content */
    .tabcontent {
        display: none;
        padding: 6px 12px;
        border: 1px solid #ccc;
        border-top: none;
     } 
     
     .tabcontent table {
        width:100%;
     }

     .tabcontent div#TabActionSection{
        clear:both;
     }
    </style>