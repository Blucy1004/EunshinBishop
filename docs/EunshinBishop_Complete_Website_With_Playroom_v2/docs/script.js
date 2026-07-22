const header=document.querySelector(".site-header");
const menuButton=document.querySelector(".menu-button");
const navigation=document.querySelector(".site-nav");
const navigationLinks=document.querySelectorAll(".site-nav a");
const revealElements=document.querySelectorAll(".reveal");
const yearElement=document.querySelector("#current-year");

function updateHeader(){
  header?.classList.toggle("scrolled",window.scrollY>20);
}
function closeMenu(){
  if(!menuButton||!navigation)return;
  menuButton.classList.remove("active");
  navigation.classList.remove("open");
  menuButton.setAttribute("aria-expanded","false");
  document.body.classList.remove("menu-open");
}
menuButton?.addEventListener("click",()=>{
  if(!navigation)return;
  const isOpen=navigation.classList.toggle("open");
  menuButton.classList.toggle("active",isOpen);
  menuButton.setAttribute("aria-expanded",String(isOpen));
  document.body.classList.toggle("menu-open",isOpen);
});
navigationLinks.forEach(link=>link.addEventListener("click",closeMenu));
window.addEventListener("scroll",updateHeader,{passive:true});
window.addEventListener("resize",()=>{if(window.innerWidth>960)closeMenu()});

if("IntersectionObserver" in window){
  const observer=new IntersectionObserver((entries,obs)=>{
    entries.forEach(entry=>{
      if(!entry.isIntersecting)return;
      entry.target.classList.add("visible");
      obs.unobserve(entry.target);
    });
  },{threshold:.12,rootMargin:"0px 0px -40px"});
  revealElements.forEach(el=>observer.observe(el));
}else{
  revealElements.forEach(el=>el.classList.add("visible"));
}
if(yearElement)yearElement.textContent=new Date().getFullYear();
updateHeader();
