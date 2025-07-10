(function (doc, win) {
	var docEl = doc.documentElement,
		docBody = doc.body,
		resizeEvt = 'orientationchange' in window ? 'orientationchange' : 'resize',
		recalc = function () {
			var clientWidth = docEl.clientWidth;
			if (!clientWidth) return;

			clientWidth = Math.min(Math.max(clientWidth, 320), 750);
			docEl.style.fontSize = 100 * (clientWidth / 320) + 'px';
		};
	if (!doc.addEventListener) return;
	docEl && recalc();
	doc.addEventListener('DOMContentLoaded', recalc, false);
	win.addEventListener(resizeEvt, recalc, false);
})(document, window);