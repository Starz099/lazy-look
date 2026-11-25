(function () {
  'use strict';

  const $ = (sel, root = document) => root.querySelector(sel);
  const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));
  const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

  /* ------------------------------------------------------------
     Header
     ------------------------------------------------------------ */
  const header = $('#siteHeader');
  const menuBtn = $('.menu-btn');
  const mobileMenu = $('#mobileMenu');

  if (header) {
    const onScroll = () => header.classList.toggle('scrolled', window.scrollY > 12);
    onScroll();
    window.addEventListener('scroll', onScroll, { passive: true });
  }

  if (menuBtn && mobileMenu) {
    const closeMenu = () => {
      menuBtn.setAttribute('aria-expanded', 'false');
      header.classList.remove('menu-open');
      document.body.classList.remove('menu-open');
    };

    menuBtn.addEventListener('click', () => {
      const isOpen = menuBtn.getAttribute('aria-expanded') !== 'true';
      menuBtn.setAttribute('aria-expanded', String(isOpen));
      header.classList.toggle('menu-open', isOpen);
      document.body.classList.toggle('menu-open', isOpen);
    });

    $$('a', mobileMenu).forEach((link) => link.addEventListener('click', closeMenu));
    window.addEventListener('keydown', (event) => {
      if (event.key === 'Escape') closeMenu();
    });
    window.addEventListener('resize', () => {
      if (window.innerWidth > 900) closeMenu();
    });
  }

  /* ------------------------------------------------------------
     Scroll reveal
     ------------------------------------------------------------ */
  const revealItems = $$('.reveal');

  if ('IntersectionObserver' in window) {
    const revealObserver = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            entry.target.classList.add('in');
            revealObserver.unobserve(entry.target);
          }
        });
      },
      { threshold: 0.12, rootMargin: '0px 0px -7% 0px' }
    );

    revealItems.forEach((item) => revealObserver.observe(item));
  } else {
    revealItems.forEach((item) => item.classList.add('in'));
  }

  /* ------------------------------------------------------------
     Animated search demo
     ------------------------------------------------------------ */
  const typedEl = $('#typedQuery');
  const resultsEl = $('#demoResults');
  const indexedEl = $('#indexedCount');

  const scenarios = [
    {
      query: 'budget report from last quarter',
      indexed: '133 files indexed',
      results: [
        {
          icon: 'file',
          title: 'Q3-<em>budget</em>-<em>report</em>.xlsx',
          meta: '~/Documents/Finance · 2.4 MB',
          tag: 'XLSX'
        },
        {
          icon: 'file',
          title: 'expenses-2026.csv',
          meta: '~/Documents/Finance · 128 KB',
          tag: 'CSV'
        },
        {
          icon: 'image',
          title: 'revenue-chart.png',
          meta: '~/Pictures/Work · Apr 12',
          tag: 'PNG'
        }
      ]
    },
    {
      query: 'that talk about distributed systems',
      indexed: '1,284 files indexed',
      results: [
        {
          icon: 'film',
          title: '<em>distributed</em>-<em>systems</em>-talk.mp4',
          meta: '~/Videos/Talks · 48:21 · 1080p',
          tag: 'MP4'
        },
        {
          icon: 'code',
          title: 'raft-consensus-notes.md',
          meta: '~/Code/notes · 14 KB',
          tag: 'MD'
        },
        {
          icon: 'file',
          title: 'designing-data-intensive-apps.pdf',
          meta: '~/Books · Page 214',
          tag: 'PDF'
        }
      ]
    },
    {
      query: 'screenshot of the new dashboard',
      indexed: '2,071 files indexed',
      results: [
        {
          icon: 'image',
          title: '<em>dashboard</em>-v2.png',
          meta: '~/Pictures/Screenshots · 2560×1440',
          tag: 'PNG'
        },
        {
          icon: 'file',
          title: 'ui-review.pptx',
          meta: '~/Work/Design · Slide 4',
          tag: 'PPTX'
        },
        {
          icon: 'file',
          title: 'notes-2026-04-18.md',
          meta: '~/Notes · Apr 18',
          tag: 'MD'
        }
      ]
    }
  ];

  const iconMarkup = (name) =>
    `<svg class="icon" aria-hidden="true"><use href="#i-${name}"></use></svg>`;

  const renderResults = (items) => {
    if (!resultsEl) return;
    resultsEl.innerHTML = items
      .map(
        (item, index) => `
        <div class="demo-result${index === 0 ? ' active' : ''}" style="transition-delay:${40 + index * 90}ms">
          <div class="res-icon">${iconMarkup(item.icon)}</div>
          <div class="res-main">
            <div class="res-title">${item.title}</div>
            <div class="res-meta">${item.meta}</div>
          </div>
          <span class="res-tag">${item.tag}</span>
          <span class="res-enter" aria-hidden="true">↵</span>
        </div>`
      )
      .join('');

    requestAnimationFrame(() => {
      requestAnimationFrame(() => {
        $$('.demo-result', resultsEl).forEach((row) => row.classList.add('in'));
      });
    });
  };

  if (typedEl && resultsEl) {
    if (reduceMotion) {
      typedEl.textContent = scenarios[0].query;
      if (indexedEl) indexedEl.textContent = scenarios[0].indexed;
      renderResults(scenarios[0].results);
    } else {
      const typeText = async (text) => {
        typedEl.textContent = '';
        for (let i = 0; i < text.length; i += 1) {
          typedEl.textContent += text[i];
          await wait(24 + Math.random() * 34);
        }
      };

      const runDemo = async () => {
        let index = 0;
        while (true) {
          const scenario = scenarios[index % scenarios.length];

          $$('.demo-result', resultsEl).forEach((row) => row.classList.remove('in'));
          await wait(380);
          resultsEl.innerHTML = '';
          typedEl.textContent = '';

          await typeText(scenario.query);
          if (indexedEl) indexedEl.textContent = scenario.indexed;
          await wait(220);
          renderResults(scenario.results);
          await wait(5200);

          index += 1;
        }
      };

      runDemo();
    }
  }

  /* ------------------------------------------------------------
     Particle lattice
     ------------------------------------------------------------ */
  const canvas = $('#lattice');

  if (canvas && canvas.getContext) {
    const ctx = canvas.getContext('2d');
    let width = 0;
    let height = 0;
    let dpr = 1;
    let points = [];
    let frame = 0;
    let isVisible = true;

    const buildPoints = () => {
      points = [];
      const count = Math.min(1300, Math.max(520, Math.round(width * 1.45)));
      const inner = 0.6;
      const bandCount = Math.round(count * 0.86);

      const pushPoint = (radius, thickness, sizeMin, sizeRange, alphaMin, alphaRange) => {
        const edge = Math.floor(Math.random() * 4);
        const along = Math.random() * 2 - 1;
        let x;
        let z;

        if (edge === 0) {
          x = along * radius;
          z = -radius;
        } else if (edge === 1) {
          x = radius;
          z = along * radius;
        } else if (edge === 2) {
          x = along * radius;
          z = radius;
        } else {
          x = -radius;
          z = along * radius;
        }

        points.push({
          x,
          y: (Math.random() - 0.5) * thickness,
          z,
          alpha: alphaMin + Math.random() * alphaRange,
          size: sizeMin + Math.random() * sizeRange
        });
      };

      for (let i = 0; i < bandCount; i += 1) {
        const depth = Math.random();
        const radius = 1 - depth * (1 - inner);
        pushPoint(radius, 0.075, 0.85, 0.75, 0.34, 0.66);
      }

      for (let i = 0; i < count - bandCount; i += 1) {
        const radius = 1.18 + Math.random() * 0.1;
        pushPoint(radius, 0.16, 0.6, 1.15, 0.16, 0.5);
      }
    };

    const resize = () => {
      const rect = canvas.getBoundingClientRect();
      if (!rect.width || !rect.height) return;
      dpr = Math.min(window.devicePixelRatio || 1, 2);
      width = rect.width;
      height = rect.height;
      canvas.width = Math.round(width * dpr);
      canvas.height = Math.round(height * dpr);
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      buildPoints();
      if (reduceMotion) draw(performance.now());
    };

    const draw = (now) => {
      if (!width || !height) return;
      ctx.clearRect(0, 0, width, height);

      const time = now / 1000;
      const angle = reduceMotion ? 0.7 : time * 0.17;
      const bob = reduceMotion ? 0 : Math.sin(time * 0.55) * 6;
      const scale = Math.min(width, height) * 0.44;
      const cosA = Math.cos(angle);
      const sinA = Math.sin(angle);
      const tilt = 0.56;
      const cosT = Math.cos(tilt);
      const sinT = Math.sin(tilt);

      const projected = points.map((point) => {
        const xr = point.x * cosA - point.z * sinA;
        const zr = point.x * sinA + point.z * cosA;
        const yr = point.y * cosT - zr * sinT;
        const depth = point.y * sinT + zr * cosT;

        return {
          x: width / 2 + xr * scale,
          y: height / 2 + yr * scale + bob + 8,
          depth,
          point
        };
      });

      projected.sort((a, b) => a.depth - b.depth);

      ctx.globalCompositeOperation = 'lighter';

      for (let i = 0; i < projected.length; i += 1) {
        const item = projected[i];
        const norm = Math.max(0, Math.min(1, (item.depth + 1.2) / 2.4));
        const alpha = (0.16 + norm * 0.62) * item.point.alpha;
        const radius = Math.max(0.45, item.point.size * (0.7 + norm * 0.6));

        ctx.beginPath();
        ctx.arc(item.x, item.y, radius * 2.4, 0, Math.PI * 2);
        ctx.fillStyle = `rgba(174, 255, 46, ${alpha * 0.1})`;
        ctx.fill();

        ctx.beginPath();
        ctx.arc(item.x, item.y, radius, 0, Math.PI * 2);
        ctx.fillStyle = `rgba(${205 + Math.round(norm * 40)}, 255, ${60 + Math.round(norm * 55)}, ${alpha})`;
        ctx.fill();
      }

      ctx.globalCompositeOperation = 'source-over';
    };

    const loop = (now) => {
      draw(now);
      frame = requestAnimationFrame(loop);
    };

    const start = () => {
      if (reduceMotion) {
        draw(performance.now());
        return;
      }
      if (frame) return;
      frame = requestAnimationFrame(loop);
    };

    const stop = () => {
      if (frame) {
        cancelAnimationFrame(frame);
        frame = 0;
      }
    };

    resize();
    start();

    if ('ResizeObserver' in window) {
      const resizeObserver = new ResizeObserver(() => resize());
      resizeObserver.observe(canvas);
    } else {
      window.addEventListener('resize', resize);
    }

    if ('IntersectionObserver' in window && !reduceMotion) {
      const canvasObserver = new IntersectionObserver(
        (entries) => {
          entries.forEach((entry) => {
            isVisible = entry.isIntersecting;
            if (isVisible) {
              start();
            } else {
              stop();
            }
          });
        },
        { threshold: 0 }
      );
      canvasObserver.observe(canvas);

      document.addEventListener('visibilitychange', () => {
        if (document.hidden) {
          stop();
        } else if (isVisible) {
          start();
        }
      });
    }
  }

  /* ------------------------------------------------------------
     Footer year
     ------------------------------------------------------------ */
  const yearEl = $('#year');
  if (yearEl) yearEl.textContent = String(new Date().getFullYear());
})();
