<script setup>
import { useRoute } from 'vue-router';
import { computed } from 'vue';
import AppNavbar from '@/components/AppNavbar.vue';

const route = useRoute();
const isAuthPage = computed(() => ['login', 'register'].includes(route.name));
</script>

<template>
  <div class="app-shell">
    <AppNavbar v-if="!isAuthPage" />
    <router-view />
  </div>
</template>
