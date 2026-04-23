const root = document.documentElement;
const revealNodes = document.querySelectorAll(".reveal");
const progressBar = document.querySelector(".scroll-progress");
const yearNodes = document.querySelectorAll("[data-year]");
const pageNavLinks = document.querySelectorAll(".topnav a[data-page-link]");
const spyContainers = document.querySelectorAll("[data-spy-nav]");
const sections = [...document.querySelectorAll("main section[id]")];
const topbar = document.querySelector(".topbar");

for (const node of yearNodes) {
  node.textContent = String(new Date().getFullYear());
}

function normalizePathname(value) {
  if (!value) {
    return "index.html";
  }

  let normalized = value.replace(/\\/g, "/");
  if (normalized.endsWith("/")) {
    normalized += "index.html";
  }

  const parts = normalized.split("/").filter(Boolean);
  return parts.length ? parts[parts.length - 1].toLowerCase() : "index.html";
}

function updatePageNav() {
  if (!pageNavLinks.length) {
    return;
  }

  const current = normalizePathname(window.location.pathname);
  for (const link of pageNavLinks) {
    const target = normalizePathname(link.getAttribute("href"));
    const active = current === target;
    link.classList.toggle("is-page-active", active);
    if (active) {
      link.setAttribute("aria-current", "page");
    } else {
      link.removeAttribute("aria-current");
    }
  }
}

function updateScrollProgress() {
  if (!progressBar) {
    return;
  }

  const total = document.documentElement.scrollHeight - window.innerHeight;
  const ratio = total > 0 ? Math.min(window.scrollY / total, 1) : 0;
  progressBar.style.width = `${ratio * 100}%`;
}

function updateSectionSpy() {
  if (!spyContainers.length || !sections.length) {
    return;
  }

  const offset = window.scrollY + 160;
  let activeId = sections[0].id;

  for (const section of sections) {
    if (section.offsetTop <= offset) {
      activeId = section.id;
    }
  }

  for (const container of spyContainers) {
    const links = container.querySelectorAll("a[href^='#']");
    for (const link of links) {
      const targetId = link.getAttribute("href").slice(1);
      link.classList.toggle("is-active", targetId === activeId);
    }
  }
}

function scrollToHashTarget(behavior = "auto") {
  if (!window.location.hash) {
    return;
  }

  const targetId = decodeURIComponent(window.location.hash.slice(1));
  if (!targetId) {
    return;
  }

  const target = document.getElementById(targetId);
  if (!target) {
    return;
  }

  target.classList.add("is-visible");

  const topbarHeight = topbar ? topbar.getBoundingClientRect().height : 0;
  const offset = topbarHeight + 24;
  const top = target.getBoundingClientRect().top + window.scrollY - offset;
  window.scrollTo({
    top: Math.max(top, 0),
    behavior,
  });
}

function showAllRevealNodes() {
  for (const node of revealNodes) {
    node.classList.add("is-visible");
  }
  root.classList.remove("js-enhanced");
}

function initRevealObserver() {
  if (!revealNodes.length) {
    return;
  }

  root.classList.add("js-enhanced");

  if (!("IntersectionObserver" in window)) {
    showAllRevealNodes();
    return;
  }

  const revealObserver = new IntersectionObserver(
    entries => {
      for (const entry of entries) {
        if (entry.isIntersecting) {
          entry.target.classList.add("is-visible");
          revealObserver.unobserve(entry.target);
        }
      }
    },
    {
      threshold: 0.12,
      rootMargin: "0px 0px -5% 0px",
    }
  );

  for (const node of revealNodes) {
    revealObserver.observe(node);
  }

  // If observers fail to report any visible section, keep content readable.
  window.setTimeout(() => {
    const hasVisibleNode = [...revealNodes].some(node => node.classList.contains("is-visible"));
    if (!hasVisibleNode) {
      showAllRevealNodes();
    }
  }, 1200);
}

window.addEventListener("scroll", () => {
  updateScrollProgress();
  updateSectionSpy();
});

window.addEventListener("hashchange", () => {
  window.setTimeout(() => {
    scrollToHashTarget("smooth");
    updateSectionSpy();
  }, 0);
});

window.addEventListener("load", () => {
  window.setTimeout(() => {
    scrollToHashTarget();
    updateScrollProgress();
    updateSectionSpy();
  }, 0);
});

window.addEventListener("DOMContentLoaded", () => {
  try {
    initRevealObserver();
    updatePageNav();
    updateScrollProgress();
    updateSectionSpy();
    window.setTimeout(() => {
      scrollToHashTarget();
      updateScrollProgress();
      updateSectionSpy();
    }, 0);
  } catch (error) {
    showAllRevealNodes();
    console.error(error);
  }
});
