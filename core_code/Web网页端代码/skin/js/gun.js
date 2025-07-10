function HScroll(domid,lid,rid,time){
		        function E(domid){
			        return typeof domid=="object" ? domid:document.getElementById(domid);
		        }
		        domid=E(domid);
		        lid=E(lid);
		        rid=E(rid);
		        var t=this;
		        t.left=0;
		        t.step=1;
		        t.init=function(){
			        t.addevent();
			        $("#pro_list").css({'overflow':'hidden'});
			        var w=($("#pro_list>ul li").width()+14)*$("#pro_list>ul").children().length;
			        domid.swrapw=w;
			        $("#pro_list ul").append($("#pro_list ul").html()).width(2*w);
			        domid.duration="right";
			        t.scroll();
		        };
		        t.addevent=function(){
			        $(lid).click(function(){
				        domid.duration="right";
			        });
			        $(rid).click(function(){
				        domid.duration="left";
			        });
			        $(domid).mouseenter(function(){
				        window.clearTimeout(t.timer);
			        }).mouseleave(function(){
				        t.scroll();
			        });
		        };
		        t.scroll=function(){
        			
			        t.timer=window.setTimeout(function(){
				        if(domid.duration=="right"){
					        t.left+=t.step;
				            if(t.left>=domid.swrapw){
						        t.left=0;
					        }
				        }else{
					        t.left-=t.step;
					        if(t.left<=0){
						        t.left=domid.swrapw;
					        }
				        }
				        domid.scrollLeft=t.left;
        				
				        t.timer=window.setTimeout(arguments.callee,30);

			        },30);
		        }
		        t.init();
	        }
	        new HScroll("pro_list","pro_left","pro_right");