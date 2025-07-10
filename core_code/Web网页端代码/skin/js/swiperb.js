var swiper = new Swiper('.swiper1', {
	pagination: '.swiper-pagination',
	paginationClickable: true,
	spaceBetween: 0,
	autoplay: 3000,
	autoplayDisableOnInteraction : false,//滑动后仍自动播放
});
var swiper = new Swiper('.swiper2', {
   pagination: '.swiper-pagination',
	slidesPerView: 4,//一行的个数
	slidesPerColumn: 1,//分几行
	slidesPerGroup : 1,//滑动的组
	paginationClickable: true,
	spaceBetween: 30,
	autoplay: 2500,
	speed:1000,
	autoplayDisableOnInteraction : false,//滑动后仍自动播放
});
var swiper = new Swiper('.swiper3', {
   pagination: '.swiper-pagination',
	slidesPerView: 5,//一行的个数
	slidesPerColumn: 1,//分几行
	slidesPerGroup : 5,//滑动的组
	paginationClickable: true,
	spaceBetween: 30,
	autoplay: 4000,
	speed:1000,
	autoplayDisableOnInteraction : false,//滑动后仍自动播放
});
